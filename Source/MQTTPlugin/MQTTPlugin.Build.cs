using System;
using System.IO;
using UnrealBuildTool;

public class MQTTPlugin : ModuleRules
{
    private string ModulePath
    {
        get { return ModuleDirectory; }
    }

    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty")); }
    }

    private int HashFile(string FilePath)
    {
        string DLLString = File.ReadAllText(FilePath);
        return DLLString.GetHashCode() + DLLString.Length;  //ensure both hash and file lengths match
    }

    public string GetUProjectPath()
    {
        return Path.Combine(ModuleDirectory, "../../../..");
    }

    private string CopyToProjectBinaries(string Filepath, ReadOnlyTargetRules Target)
    {
        string BinariesDir = Path.Combine(GetUProjectPath(), "Binaries", Target.Platform.ToString());
        string Filename = Path.GetFileName(Filepath);

        //convert relative path 
        string FullBinariesDir = Path.GetFullPath(BinariesDir);

        if (!Directory.Exists(FullBinariesDir))
        {
            Directory.CreateDirectory(FullBinariesDir);
        }

        string FullExistingPath = Path.Combine(FullBinariesDir, Filename);
        bool ValidFile = false;

        //File exists, check if they're the same
        if (File.Exists(FullExistingPath))
        {
            int ExistingFileHash = HashFile(FullExistingPath);
            int TargetFileHash = HashFile(Filepath);
            ValidFile = ExistingFileHash == TargetFileHash;
            if (!ValidFile)
            {
                System.Console.WriteLine("PahoMQTT: outdated dll detected.");
            }
        }

        //No valid existing file found, copy new dll
        if (!ValidFile)
        {
            System.Console.WriteLine("PahoMQTT: Copied from " + Filepath + ", to " + Path.Combine(FullBinariesDir, Filename));
            File.Copy(Filepath, Path.Combine(FullBinariesDir, Filename), true);
        }

        return FullExistingPath;
    }

    public MQTTPlugin(ReadOnlyTargetRules TargetRules) : base(TargetRules)
    {
        PrivatePCHHeaderFile = "Public/MQTTPlugin.h";

        PublicIncludePaths.Add(String.Format("{0}/PahoMQTT/src", ThirdPartyPath));

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Engine",
                "Core",
                "CoreUObject",
                "InputCore",
                "Networking",
                "Sockets",
            });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicLibraryPaths.Add(String.Format("{0}/PahoMQTT/build/src/Release", ThirdPartyPath));
            PublicAdditionalLibraries.Add("paho-mqtt3a.lib");
            PublicAdditionalLibraries.Add("paho-mqtt3c.lib");

            var DLLs = new string[] {
                "paho-mqtt3a.dll",
                "paho-mqtt3c.dll"
            };

            PublicDelayLoadDLLs.AddRange(DLLs);
            string BaseBinaryDirectory = String.Format("{0}/PahoMQTT/build/src/Release", ThirdPartyPath);

            foreach (var DLL in DLLs)
            {
                var ThirdPartyDLLPath = String.Format("{0}/{1}", BaseBinaryDirectory, DLL);
                CopyToProjectBinaries(ThirdPartyDLLPath, Target);
                string DLLPath = Path.GetFullPath(Path.Combine(GetUProjectPath(), "Binaries", Target.Platform.ToString(), DLL));
                RuntimeDependencies.Add(DLLPath);
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            var BaseBinaryDirectory = String.Format("{0}/PahoMQTT/build/output/", ThirdPartyPath);

            PublicLibraryPaths.Add(BaseBinaryDirectory);
            PublicAdditionalLibraries.Add("paho-mqtt3a");
            PublicAdditionalLibraries.Add("paho-mqtt3as");
            PublicAdditionalLibraries.Add("paho-mqtt3c");
            PublicAdditionalLibraries.Add("paho-mqtt3cs");
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            var AndroidBinaryPath = String.Format("{0}/PahoMQTT/obj/local/", ThirdPartyPath);
            PublicLibraryPaths.Add(AndroidBinaryPath + "armeabi-v7a");
            PublicLibraryPaths.Add(AndroidBinaryPath + "arm64-v8a");
            PublicLibraryPaths.Add(AndroidBinaryPath + "x86");
            PublicLibraryPaths.Add(AndroidBinaryPath + "x86_64");
            PublicAdditionalLibraries.Add("PahoMQTT");
        }
    }
}
