// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2009-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//  Copyright (C) 2015 Gunter Königsmann     <wxMaxima@physikbuch.de>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//
//  SPDX-License-Identifier: GPL-2.0+

/*! \file
  This file declares the class AutoComplete.

  AutoComplete creates the list of autocompletions for a string and allows
  dynamically appending maxima commands to this list as soon as they are defined.
*/

#ifndef AUTOCOMPLETE_H
#define AUTOCOMPLETE_H

#include <thread>
#include <algorithm>
#include <memory>
#include <mutex>
#include <wx/wx.h>
#include <wx/xml/xml.h>
#include <wx/event.h>
#include <wx/dir.h>
#include <vector>
#include <wx/object.h>
#include <wx/regex.h>
#include <wx/filename.h>
#include <wx/hashmap.h>
#include "Configuration.h"
#include "precomp.h"
#include "Version.h"
#include <unordered_map>
/* The autocompletion logic

   The wordlists for autocompletion for keywords come from several sources:

   - wxMaxima::ReadLoadSymbols receive the contents of maxima's variables
   "values" and "functions" after a package is loaded.
   - all words that appear in the worksheet
   - and a list of maxima's builtin commands.
*/
class AutoComplete : public wxEvtHandler
{
  typedef std::unordered_map <wxString, int, wxStringHash> WorksheetWords;
public:
  using WordList = std::vector<wxString>;

  //! All types of things we can autocomplete
  enum autoCompletionType
  {
    command = 0, //! Command names. \attention Must be the first entry in this enum
    tmplte,  //! Function templates
    loadfile,//! loadable files
    demofile,//! loadable files
    generalfile,//! general files
    esccommand, //! Esc commands describing symbols
    unit,    //! Unit names. \attention Must be the last entry in this enum
    numberOfTypes //! Not a completion type, but the marker for how many types there are
  };
  explicit AutoComplete(Configuration *configuration);

  //! The destructor of AutoComplete
  virtual ~AutoComplete();

  //! Load all autocomplete symbols wxMaxima knows about by itself
  void LoadSymbols();

  /*! Makes wxMaxima know all its builtin symbols.

    This function might cause a compiler warning because it is
    suspiciously long.
    For the same reason it has been split into a separate file.
  */
  void LoadBuiltinSymbols();

  //! Manually add an autocompletable symbol to our symbols lists
  void AddSymbol(wxString fun, autoCompletionType type = command);
  //! Interprets the XML autocompletable symbol list maxima can send us
  void AddSymbols(wxString xml);
  //! Interprets the XML autocompletable symbol list maxima can send us
  void AddSymbols(wxXmlDocument xml);
  //! The real work of AddSymbols is made here and in the background
  void AddSymbols_Backgroundtask_string(wxString xml);
  //! The real work of AddSymbols is made here and in the background
  void AddSymbols_Backgroundtask(wxXmlDocument xmldoc);


  //! Replace the list of files in the directory the worksheet file is in to the demo files list
  void UpdateDemoFiles(wxString partial, const wxString &maximaDir);
  //! Replace the list of files in the directory the worksheet file is in to the load files list
  void UpdateLoadFiles(wxString partial, const wxString &maximaDir);
  //! Assemble a list of files
  void UpdateGeneralFiles(wxString partial, const wxString &maximaDir);

  //! Add words to the list of words that appear in the workSheet's code cells
  void AddWorksheetWords(const WordList &words);
  void AddWorksheetWords(WordList::const_iterator begin, WordList::const_iterator end);

  //! Clear the list of words that appear in the workSheet's code cells
  void ClearWorksheetWords();
  //! Clear the list of files demo() can be applied on
  void ClearDemofileList();

  //! Returns a list of possible autocompletions for the string "partial"
  std::vector<wxString> CompleteSymbol(wxString partial, autoCompletionType type = command);
  //! Basically runs a regex over templates
  static wxString FixTemplate(wxString templ);
  //! Returns a list of demo files we know of
  std::vector<wxString> GetDemoFilesList();
  //! Returns a list of Symbols we know
  std::vector<wxString> GetSymbolList();
  //! Does a demo file for this command exist?
  bool HasDemofile(const wxString &commandname);

private:
  //! The configuration storage
  Configuration *m_configuration;
  //! Loads the list of loadable files and can be run in a background task
  void LoadableFiles_BackgroundTask(wxString sharedir, wxString demodir);
  //! Prepares the list of built-in symbols and can be run in a background task
  void BuiltinSymbols_BackgroundTask();

  //! Replace the list of files in the directory the worksheet file is in to the load files list
  void UpdateLoadFiles_BackgroundTask(wxString partial, wxString maximaDir);
  //! The list of loadable files maxima provides
  std::vector<wxString> m_builtInLoadFiles;
  //! The list of demo files maxima provides
  std::vector<wxString> m_builtInDemoFiles;

  //! Scans the maxima directory for a list of loadable files
  class GetGeneralFiles : public wxDirTraverser
  {
  public:
    explicit GetGeneralFiles(std::vector<wxString>& files,
                             std::mutex *lock,
                             const wxString &prefix = wxEmptyString) :
      m_files(files), m_lock(lock), m_prefix(prefix) { }
    wxDirTraverseResult OnFile(const wxString& filename) override
      {
        wxFileName newItemName(filename);
        wxString newItem = "\"" + m_prefix + newItemName.GetFullName() + "\"";
        newItem.Replace(wxFileName::GetPathSeparator(), "/");
        {
          const std::lock_guard<std::mutex> lock(*m_lock);
          if(std::find(m_files.begin(), m_files.end(), newItem) == m_files.end())
            m_files.push_back(newItem);
        }
        return wxDIR_CONTINUE;
      }
    wxDirTraverseResult OnDir(const wxString& dirname) override
      {
        wxFileName newItemName(dirname);
        wxString newItem = "\"" + m_prefix + newItemName.GetFullName() + "/\"";
        newItem.Replace(wxFileName::GetPathSeparator(), "/");
        {
          const std::lock_guard<std::mutex> lock(*m_lock);
          if(std::find(m_files.begin(), m_files.end(), newItem) == m_files.end())
            m_files.push_back(newItem);
        }
        return wxDIR_IGNORE;
      }
    std::vector<wxString> GetResult(){
      const std::lock_guard<std::mutex> lock(*m_lock);
      return m_files;
    }
  protected:
    std::vector<wxString>& m_files;
    std::mutex *m_lock;
    wxString m_prefix;
  };

  //! Recursively scans the maxima directory for a list of .mac files
  class GetMacFiles_includingSubdirs : public wxDirTraverser
  {
  public:
    explicit GetMacFiles_includingSubdirs(std::vector<wxString>& files,
                                          std::mutex *lock,
                                          const wxString &prefix = wxEmptyString) :
      m_files(files), m_lock(lock), m_prefix(prefix)  { }
    wxDirTraverseResult OnFile(const wxString& filename) override
      {
        if(
          (filename.EndsWith(".mac"))||
          (filename.EndsWith(".lisp"))||
          (filename.EndsWith(".wxm"))
          )
        {
          wxFileName newItemName(filename);
          wxString newItem = "\"" + m_prefix + newItemName.GetName() + "\"";
          newItem.Replace(wxFileName::GetPathSeparator(), "/");
          {
            const std::lock_guard<std::mutex> lock(*m_lock);
            if(std::find(m_files.begin(), m_files.end(), newItem) == m_files.end())
              m_files.push_back(newItem);
          }
        }
        return wxDIR_CONTINUE;
      }
    wxDirTraverseResult OnDir(const wxString& dirname) override
      {
        if((dirname.EndsWith(".git")) ||
           (dirname.EndsWith("/share/share")) ||
           (dirname.EndsWith("/src/src")) ||
           (dirname.EndsWith("/doc/doc")) ||
           (dirname.EndsWith("/interfaces/interfaces"))
          )
          return wxDIR_STOP;
        else
          return wxDIR_CONTINUE;
      }
    std::vector<wxString> GetResult(){
      const std::lock_guard<std::mutex> lock(*m_lock);
      return m_files;
    }
  protected:
    std::vector<wxString>& m_files;
    std::mutex *m_lock;
    wxString m_prefix;
  };

  //! Scans the user directory for a list of .mac files
  class GetMacFiles : public GetMacFiles_includingSubdirs
  {
  public:
    explicit GetMacFiles(std::vector<wxString>& files,
                         std::mutex *lock,
                         const wxString &prefix = wxEmptyString) :
      GetMacFiles_includingSubdirs(files, lock, prefix){ }
    wxDirTraverseResult OnDir(const wxString& dirname) override
      {
        wxFileName newItemName(dirname);
        wxString newItem = "\"" + m_prefix + newItemName.GetFullName() + "/\"";
        newItem.Replace(wxFileName::GetPathSeparator(), "/");
        {
          const std::lock_guard<std::mutex> lock(*m_lock);
          if(std::find(m_files.begin(), m_files.end(), newItem) == m_files.end())
            m_files.push_back(newItem);
        }
        return wxDIR_IGNORE;
      }
  };

  //! Scans a directory for a list of demo files
  class GetDemoFiles_includingSubdirs : public wxDirTraverser
  {
  public:
    explicit GetDemoFiles_includingSubdirs(std::vector<wxString>& files,
                                           std::mutex *lock,
                                           const wxString &prefix = wxEmptyString) :
      m_files(files), m_lock(lock), m_prefix(prefix) { }
    wxDirTraverseResult OnFile(const wxString& filename) override
      {
        if(filename.EndsWith(".dem"))
        {
          wxFileName newItemName(filename);
          wxString newItem = "\"" + m_prefix + newItemName.GetName() + "\"";
          newItem.Replace(wxFileName::GetPathSeparator(), "/");
          {
            const std::lock_guard<std::mutex> lock(*m_lock);
            if(std::find(m_files.begin(), m_files.end(), newItem) == m_files.end())
              m_files.push_back(newItem);
          }
        }
        return wxDIR_CONTINUE;
      }
    wxDirTraverseResult OnDir(const wxString& dirname) override
      {
        if((dirname.EndsWith(".git")) ||
           (dirname.EndsWith("/share/share")) ||
           (dirname.EndsWith("/src/src")) ||
           (dirname.EndsWith("/doc/doc")) ||
           (dirname.EndsWith("/interfaces/interfaces"))
          )
          return wxDIR_STOP;
        else
          return wxDIR_CONTINUE;
      }
    std::vector<wxString> GetResult(){
      const std::lock_guard<std::mutex> lock(*m_lock);
      return m_files;
    }
  protected:
    std::vector<wxString>& m_files;
    std::mutex *m_lock;
    wxString m_prefix;
  };

  //! Scans the maxima directory for a list of demo files
  class GetDemoFiles : public GetDemoFiles_includingSubdirs
  {
  public:
    explicit GetDemoFiles(std::vector<wxString>& files,
                          std::mutex *lock,
                          const wxString &prefix = wxEmptyString) :
      GetDemoFiles_includingSubdirs(files, lock, prefix){ }
    virtual wxDirTraverseResult OnDir(const wxString& dirname) override
      {
        wxFileName newItemName(dirname);
        wxString newItem = "\"" + m_prefix + newItemName.GetFullName() + "/\"";
        newItem.Replace(wxFileName::GetPathSeparator(), "/");
        {
          const std::lock_guard<std::mutex> lock(*m_lock);
          if(std::find(m_files.begin(), m_files.end(), newItem) == m_files.end())
            m_files.push_back(newItem);
        }
        return wxDIR_IGNORE;
      }
  };

  jthread m_addSymbols_backgroundThread;
  jthread m_addFiles_backgroundThread;
  //! Is locked when someone accesses a keyword list
  std::mutex m_keywordsLock;
  //! The lists of autocompletable symbols for the classes defined in autoCompletionType
  std::vector<std::vector<wxString>> m_wordList;
  static wxRegEx m_args;
  WorksheetWords m_worksheetWords;
};

wxDECLARE_EVENT(NEW_DEMO_FILES_EVENT, wxCommandEvent);

#endif // AUTOCOMPLETE_H
