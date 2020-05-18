#include "csharp_interface.h"

#include <fstream>
#include <sstream>
#include <regex>

#include "log.h"

#include "csharp_utils.h"
#include "base_script.h"

#using <System.Windows.Forms.dll>
#using <System.Drawing.dll>
#using <System.dll>
#using <Microsoft.JScript.dll>

using namespace MOBase;

/**
 * This is a assembly resolve handler that does only one thing: returns the assembly 
 * containing BaseScript (usually the DLL) when requested.
 *
 * I don't know why this must be done manually... But I did not find any better solution.
 */
System::Reflection::Assembly^ currentDomain_AssemblyResolve(System::Object^, System::ResolveEventArgs^ args)
{
  using namespace System::Reflection;

  try {
    Assembly^ baseScriptAssembly = System::Reflection::Assembly::GetAssembly(BaseScript::typeid);
    if (args->Name->Equals(baseScriptAssembly->FullName)) {
      return baseScriptAssembly;
    }
  }
  catch (...)
  {
  }

  return nullptr;
}

IPluginInstaller::EInstallResult executeScript(System::String^ script) {

  using namespace System;
  using namespace System::CodeDom;
  using namespace System::CodeDom::Compiler;
  using namespace System::Collections::Generic;
  using namespace System::Collections;
  using namespace System::ComponentModel;
  using namespace System::Diagnostics;
  using namespace System::Drawing;
  using namespace System::IO;
  using namespace System::Windows::Forms;
  using namespace Microsoft::CSharp;
  using namespace Microsoft::VisualBasic;
  using namespace Microsoft::JScript;
  using namespace System::Security::Permissions;

  AppDomain^ currentDomain = AppDomain::CurrentDomain;
  currentDomain->AssemblyResolve += gcnew ResolveEventHandler(currentDomain_AssemblyResolve);

  // From Nexus-Mods/fomod-installer:
  Dictionary<String^, String^>^ dicOptions = gcnew Dictionary<String^, String^>(10);
  dicOptions->Add("CompilerVersion", "v4.0");

  // List of assemblies (including BaseScript) - From Nexus-Mods/fomod-installer:
  array<String^>^ referenceAssemblies = {
    "System.dll",
    "System.Runtime.dll",
    "System.Drawing.dll",
    "System.Windows.Forms.dll",
    "System.Xml.dll",
    System::Reflection::Assembly::GetAssembly(BaseScript::typeid)->Location
  };

  CompilerParameters^ cp = gcnew CompilerParameters(referenceAssemblies);
  cp->GenerateExecutable = false;
  cp->IncludeDebugInformation = false;
  cp->GenerateInMemory = true;
  cp->TreatWarningsAsErrors = false;
  CodeDomProvider^ provider = CodeDomProvider::CreateProvider("CSharp", dicOptions);

  // Compile the script
  auto result = provider->CompileAssemblyFromSource(cp, script);

  int errorCount = 0;
  for (int i = 0; i < result->Errors->Count; ++i) {
    CompilerError^ error = result->Errors[i];
    if (error->IsWarning) {
      log::error("C# [{}]: {}", error->Line, CSharp::to_string(error->ErrorText));
    }
    else {
      log::warn("C# [{}]: {}", error->Line, CSharp::to_string(error->ErrorText));
    }
    ++errorCount;
  }

  if (errorCount > 0) {
    return IPluginInstaller::EInstallResult::RESULT_FAILED;
  }

  // Execute the script:
  try {
    auto scriptClass = result->CompiledAssembly->GetType("Script");
    BaseScript^ scriptObject = (BaseScript^)System::Activator::CreateInstance(scriptClass);
    auto success = (bool)scriptObject->GetType()->GetMethod("OnActivate")->Invoke(scriptObject, nullptr);
    return success ? IPluginInstaller::EInstallResult::RESULT_SUCCESS : IPluginInstaller::EInstallResult::RESULT_CANCELED;
  }
  catch (Exception^ ex) {
    log::error("C# ({}): {}\n{}", CSharp::to_string(ex->GetType()->FullName), CSharp::to_string(ex->Message), CSharp::to_string(ex->StackTrace));
    return IPluginInstaller::EInstallResult::RESULT_FAILED;
  }
 
}

namespace CSharp {

  IPluginInstaller::EInstallResult executeCSharpScript(QString scriptPath) {

    // Read the script:
    std::string script;

    {
      std::ifstream t(scriptPath.toStdString());
      std::stringstream buffer;
      buffer << t.rdbuf();
      script = buffer.str();
    }

    // This matches the script class to retrieve the "BaseScript" name in the file:
    std::regex m_regScriptClass(R"re((class\s+Script\s*:.*?)(\S*BaseScript))re");

    std::smatch results;
    std::regex_search(script, results, m_regScriptClass);

    // No result, no script!
    if (!results.size()) {
      log::debug("C#: Did not find 'Script' class in {}.", scriptPath);
      return IPluginInstaller::EInstallResult::RESULT_FAILED;
    }

    // Replace the base script name used by BaseScript - Slightly modified regex since
    // C++ does not support look-behind:
    std::string baseScriptName = results[2].str();
    std::regex m_regBaseScriptName(R"re((class\s+\S+\s*:.*?)[^\w])re" + baseScriptName);
    script = std::regex_replace(script, m_regBaseScriptName, "$1BaseScript");

    // This simply matches the use fomm.Scripting to remove it:
    std::regex m_regFommUsing(R"re(\s*using\s*fomm.Scripting\s*;)re");
    script = std::regex_replace(script, m_regFommUsing, "");

    String^ scriptSharp = msclr::interop::marshal_as<String^>(script);

    auto result = executeScript(scriptSharp);

    if (result != IPluginInstaller::EInstallResult::RESULT_SUCCESS) {
      return result;
    }

    return IPluginInstaller::EInstallResult::RESULT_SUCCESS;
  }

}