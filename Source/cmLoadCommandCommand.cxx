/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmLoadCommandCommand.h"
#include "cmCPluginAPI.h"
#include "cmCPluginAPI.cxx"
#include "cmDynamicLoader.h"
#include <signal.h>

// a class for loadabple commands
class cmLoadedCommand : public cmCommand
{
public:
  cmLoadedCommand() {
    memset(&this->info,0,sizeof(this->info)); 
    this->info.CAPI = &cmStaticCAPI;
  }
  
  ///! clean up any memory allocated by the plugin
  ~cmLoadedCommand();
    
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
      cmLoadedCommand *newC = new cmLoadedCommand;
      // we must copy when we clone
      memcpy(&newC->info,&this->info,sizeof(info));
      return newC;
    }

  /**
   * This is called when the command is first encountered in
   * the CMakeLists.txt file.
   */
  virtual bool InitialPass(std::vector<std::string> const& args);

  /**
   * This is called at the end after all the information
   * specified by the command is accumulated. Most commands do
   * not implement this method.  At this point, reading and
   * writing to the cache can be done.
   */
  virtual void FinalPass();

  /**
   * This determines if the command gets propagated down
   * to makefiles located in subdirectories.
   */
  virtual bool IsInherited() {
    return (info.m_Inherited != 0 ? true : false);
  }
  
  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return info.Name; }
  
  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
      if (this->info.GetTerseDocumentation)
        {
        cmLoadedCommand::InstallSignalHandlers(info.Name);
        const char* ret = info.GetTerseDocumentation(); 
        cmLoadedCommand::InstallSignalHandlers(info.Name, 1);
        return ret;
        }
      else
        {
        return "LoadedCommand without any additional documentation";
        }
    }
  static const char* LastName;
  static void TrapsForSignals(int sig)
    {
      fprintf(stderr, "CMake loaded command %s crashed with signal: %d.\n",
              cmLoadedCommand::LastName, sig);
    }
  static void InstallSignalHandlers(const char* name, int remove = 0)
    {
      cmLoadedCommand::LastName = name;
      if(!name)
        {
        cmLoadedCommand::LastName = "????";
        }
      
      if(!remove)
        {
        signal(SIGSEGV, cmLoadedCommand::TrapsForSignals);
#ifdef SIGBUS
        signal(SIGBUS,  cmLoadedCommand::TrapsForSignals);
#endif
        signal(SIGILL,  cmLoadedCommand::TrapsForSignals);
        }
      else
        {
        signal(SIGSEGV, 0);
#ifdef SIGBUS
        signal(SIGBUS,  0);
#endif
        signal(SIGILL,  0);
        }
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
      if (this->info.GetFullDocumentation)
        {
        cmLoadedCommand::InstallSignalHandlers(info.Name);
        const char* ret = info.GetFullDocumentation();
        cmLoadedCommand::InstallSignalHandlers(info.Name, 1);
        return ret;
        }
      else
        {
        return "LoadedCommand without any additional documentation";
        }
    }
  
  cmTypeMacro(cmLoadedCommand, cmCommand);

  cmLoadedCommandInfo info;
};

const char* cmLoadedCommand::LastName = 0;

bool cmLoadedCommand::InitialPass(std::vector<std::string> const& args)
{
  if (!info.InitialPass)
    {
    return true;
    }

  // clear the error string
  if (this->info.Error)
    {
    free(this->info.Error);
    }
  
  // create argc and argv and then invoke the command
  int argc = static_cast<int> (args.size());
  char **argv = 0;
  if (argc)
    {
    argv = (char **)malloc(argc*sizeof(char *));
    }
  int i;
  for (i = 0; i < argc; ++i)
    {
    argv[i] = strdup(args[i].c_str());
    }
  cmLoadedCommand::InstallSignalHandlers(info.Name);
  int result = info.InitialPass((void *)&info,(void *)this->m_Makefile,argc,argv); 
  cmLoadedCommand::InstallSignalHandlers(info.Name, 1);
  cmFreeArguments(argc,argv);
  
  if (result)
    {
    return true;
    }

  /* Initial Pass must have failed so set the error string */
  if (this->info.Error)
    {
    this->SetError(this->info.Error);
    }
  return false;
}

void cmLoadedCommand::FinalPass()
{
  if (this->info.FinalPass)
    {
    cmLoadedCommand::InstallSignalHandlers(info.Name);
    this->info.FinalPass((void *)&this->info,(void *)this->m_Makefile);
    cmLoadedCommand::InstallSignalHandlers(info.Name, 1);
    }
}

cmLoadedCommand::~cmLoadedCommand()
{
  if (this->info.Destructor)
    {
    cmLoadedCommand::InstallSignalHandlers(info.Name);
    this->info.Destructor((void *)&this->info);
    cmLoadedCommand::InstallSignalHandlers(info.Name, 1);
    }
  if (this->info.Error)
    {
    free(this->info.Error);
    }
}

// cmLoadCommandCommand
bool cmLoadCommandCommand::InitialPass(std::vector<std::string> const& args)
{
  if(args.size() < 1 )
    {
    return true;
    }
  
  // the file must exist
  std::string fullPath = cmDynamicLoader::LibPrefix();
  fullPath += "cm" + args[0] + cmDynamicLoader::LibExtension();

  // search for the file
  std::vector<std::string> path;
  for (unsigned int j = 1; j < args.size(); j++)
    {
    // expand variables
    std::string exp = args[j];
    cmSystemTools::ExpandRegistryValues(exp);
    
    // Glob the entry in case of wildcards.
    cmSystemTools::GlobDirs(exp.c_str(), path);
    }

  // Try to find the program.
  fullPath = cmSystemTools::FindFile(fullPath.c_str(), path);
  if (fullPath == "")
    {
    fullPath = "Attempt to load command failed from file : ";
    fullPath += cmDynamicLoader::LibPrefix();
    fullPath += "cm" + args[0] + cmDynamicLoader::LibExtension();
    this->SetError(fullPath.c_str());
    return false;
    }

  // try loading the shared library / dll
  cmLibHandle lib = cmDynamicLoader::OpenLibrary(fullPath.c_str());
  if(!lib)
    {
    std::string err = "Attempt to load the library ";
    err += fullPath + " failed.";
    const char* error = cmDynamicLoader::LastError();
    if ( error )
      {
      err += " Additional error info is:\n";
      err += error;
      }
    this->SetError(err.c_str());
    return false;
    }
  
  // find the init function
  std::string initFuncName = args[0] + "Init";
  CM_INIT_FUNCTION initFunction
    = (CM_INIT_FUNCTION)
    cmDynamicLoader::GetSymbolAddress(lib, initFuncName.c_str());
  if ( !initFunction )
    {
    initFuncName = "_";
    initFuncName += args[0];
    initFuncName += "Init";
    initFunction = (CM_INIT_FUNCTION)(
      cmDynamicLoader::GetSymbolAddress(lib, initFuncName.c_str()));
    }
  // if the symbol is found call it to set the name on the 
  // function blocker
  if(initFunction)
    {
    // create a function blocker and set it up
    cmLoadedCommand *f = new cmLoadedCommand();
    (*initFunction)(&f->info);
    m_Makefile->AddCommand(f);
    return true;
    }
  this->SetError("Attempt to load command failed. "
                 "No init function found.");
  return false;
}

