#include "common/common.h"
#include "common/json.h"
#include "openssl.h"
#include "macho.h"
#include "bundle.h"
#include <libgen.h>
#include <dirent.h>
#include <getopt.h>
#include <map>
const struct option options[] = {
	{ "debug",			no_argument,			NULL, 'd' },
	{ "force",			no_argument,			NULL, 'f' },
	{ "verbose",		no_argument,			NULL, 'v' },
	{ "cert",			required_argument,		NULL, 'c' },
	{ "pkey",			required_argument,		NULL, 'k' },
	{ "prov",			required_argument,		NULL, 'm' },
	{ "password",		required_argument,		NULL, 'p' },
	{ "bundleid",		required_argument,		NULL, 'b' },
	{ "bundlename",		required_argument,		NULL, 'n' },
	{ "entitlements",	required_argument,		NULL, 'e' },
	{ "output",			required_argument,		NULL, 'o' },
	{ "ziplevel",		required_argument,		NULL, 'z' },
	{ "dylib",			required_argument,		NULL, 'l' },
	{ "weak",			no_argument,			NULL, 'w' },
	{ "install",		no_argument,			NULL, 'i' },
	{ "quiet",			no_argument,			NULL, 'q' },
	{ "help",			no_argument,			NULL, 'h' },
	{ }
};

int usage()
{
	ZLog::Print("Usage: zsign [-options] [-k privkey.pem] [-m dev.prov] [-o output.ipa] file|folder\n");
	ZLog::Print("options:\n");
	ZLog::Print("-k, --pkey\t\tPath to private key or p12 file. (PEM or DER format)\n");
	ZLog::Print("-m, --prov\t\tPath to mobile provisioning profile.\n");
	ZLog::Print("-c, --cert\t\tPath to certificate file. (PEM or DER format)\n");
	ZLog::Print("-d, --debug\t\tGenerate debug output files. (.zsign_debug folder)\n");
	ZLog::Print("-f, --force\t\tForce sign without cache when signing folder.\n");
	ZLog::Print("-o, --output\t\tPath to output ipa file.\n");
	ZLog::Print("-p, --password\t\tPassword for private key or p12 file.\n");
	ZLog::Print("-b, --bundleid\t\tNew bundle id to change.\n");
	ZLog::Print("-n, --bundlename\tNew bundle name to change.\n");
	ZLog::Print("-e, --entitlements\tNew entitlements to change.\n");
	ZLog::Print("-z, --ziplevel\t\tCompressed level when output the ipa file. (0-9)\n");
	ZLog::Print("-l, --dylib\t\tPath to inject dylib file.\n");
	ZLog::Print("-w, --weak\t\tInject dylib as LC_LOAD_WEAK_DYLIB.\n");
	ZLog::Print("-i, --install\t\tInstall ipa file using ideviceinstaller command for test.\n");
	ZLog::Print("-q, --quiet\t\tQuiet operation.\n");
	ZLog::Print("-v, --version\t\tShow version.\n");
	ZLog::Print("-h, --help\t\tShow help.\n");
    ZLog::Print("-a, --addkey\t\tadd info key value.\n");
	return -1;
}


std::vector<string> splitString(std::string data,std::string split){
    std::vector<string> vector;
    char dest[data.length()];
    strcpy(dest,data.c_str());
    char *p;
    p = strtok (dest,split.c_str());
    while(p!=NULL) {
        string value=p;
        vector.push_back(value);
        p = strtok(NULL,split.c_str());
    }

    return vector;
}

std::map<string, string> readInfoParams(std::string addInfo){
    
    std:map<string, string> infoMap;
    std::vector<string> vector=splitString(addInfo, ";");
    for(int i=0;i<vector.size();i++)//size()容器中实际数据个数
    {
        std::vector<string> keyValueVector=splitString(vector[i], ":");
        for(int j=0;j<keyValueVector.size();j+=2){
            infoMap[keyValueVector[j]]=keyValueVector[j+1];
        }
    }
    return infoMap;
}
int main(int argc, char *argv[])
{
	ZTimer gtimer;

	bool bForce = false;
	bool bInstall = false;
	bool bWeakInject = false;
	uint32_t uZipLevel = 0;

	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	string strPassword;
	string strBundleId;
	string strDyLibFile;
	string strOutputFile;
	string strDisplayName;
	string strEntitlementsFile;
    string addInfo;
	int opt = 0;
	int argslot = -1;
    while (-1 != (opt = getopt_long(argc, argv, "dfvhc:k:m:o:ip:e:b:n:z:ql:w:a:", options, &argslot)))
	{
		switch (opt)
		{
		case 'd':
			ZLog::SetLogLever(ZLog::E_DEBUG);
			break;
		case 'f':
			bForce = true;
			break;
		case 'c':
			strCertFile = optarg;
			break;
		case 'k':
			strPKeyFile = optarg;
			break;
		case 'm':
			strProvFile = optarg;
			break;
		case 'p':
			strPassword = optarg;
			break;
		case 'b':
			strBundleId = optarg;
			break;
		case 'n':
			strDisplayName = optarg;
			break;
		case 'e':
			strEntitlementsFile = optarg;
			break;
		case 'l':
			strDyLibFile = optarg;
			break;
		case 'i':
			bInstall = true;
			break;
		case 'o':
			strOutputFile = GetCanonicalizePath(optarg);
			break;
		case 'z':
			uZipLevel = atoi(optarg);
			break;
		case 'w':
			bWeakInject = true;
			break;
		case 'q':
			ZLog::SetLogLever(ZLog::E_NONE);
			break;
		case 'v':
		{
			printf("version: 0.2\n");
			return 0;
		}
		break;
        case 'a':
        	addInfo = optarg;
            break;
		case 'h':
		case '?':
			return usage();
			break;
		}

		ZLog::DebugV(">>> Option:\t-%c, %s\n", opt, optarg);
	}

	if (optind >= argc)
	{
		return usage();
	}

	if (ZLog::IsDebug())
	{
		CreateFolder("./.zsign_debug");
		for (int i = optind; i < argc; i++)
		{
			ZLog::DebugV(">>> Argument:\t%s\n", argv[i]);
		}
	}
    std::map<string, string> infoMap = readInfoParams(addInfo);
    
//    //解析addinfo 成map
//    if(!addInfo.empty()){
//
//        NSData *data= [[NSString stringWithUTF8String:addInfo.c_str()] dataUsingEncoding:NSUTF8StringEncoding];
//        if(data!=NULL){
//            NSDictionary *dict = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:nil];
//            if(dict!=NULL){
//                for (NSString *key in dict){
//                    infoMap[[key UTF8String]] = [dict[key] UTF8String];
//                }
//            }
//        }
//    }
	string strPath = GetCanonicalizePath(argv[optind]);
	if (!IsFileExists(strPath.c_str()))
	{
		ZLog::ErrorV(">>> Invalid Path! %s\n", strPath.c_str());
		return -1;
	}

	bool bZipFile = false;
	if (!IsFolder(strPath.c_str()))
	{
		bZipFile = IsZipFile(strPath.c_str());
		if (!bZipFile)
		{ //macho file
			ZMachO macho;
			if (macho.Init(strPath.c_str()))
			{
				if(!strDyLibFile.empty())
				{//inject dylib
					bool bCreate = false;
					macho.InjectDyLib(bWeakInject, strDyLibFile.c_str(), bCreate);
				}
				else
				{
					macho.PrintInfo();
				}
				macho.Free();
			}
			return 0;
		}
	}

	ZTimer timer;
	ZSignAsset zSignAsset;
	if (!zSignAsset.Init(strCertFile, strPKeyFile, strProvFile, strEntitlementsFile, strPassword))
	{
		return -1;
	}

	bool bEnableCache = true;
	string strFolder = strPath;
	if (bZipFile)
	{ //ipa file
		bForce = true;
		bEnableCache = false;
		StringFormat(strFolder, "/tmp/zsign_folder_%llu", timer.Reset());
		ZLog::PrintV(">>> Unzip:\t%s (%s) -> %s ... \n", strPath.c_str(), GetFileSizeString(strPath.c_str()).c_str(), strFolder.c_str());
		RemoveFolder(strFolder.c_str());
		if (!SystemExec("unzip -qq -d '%s' '%s'", strFolder.c_str(), strPath.c_str()))
		{
			RemoveFolder(strFolder.c_str());
			ZLog::ErrorV(">>> Unzip Failed!\n");
			return -1;
		}
		timer.PrintResult(true, ">>> Unzip OK!");
	}

	timer.Reset();
	ZAppBundle bundle;
    bool bRet = bundle.SignFolder(&zSignAsset, strFolder, strBundleId, strDisplayName, strDyLibFile, bForce, bWeakInject, bEnableCache, infoMap);
	timer.PrintResult(bRet, ">>> Signed %s!", bRet ? "OK" : "Failed");

	if (bInstall && strOutputFile.empty())
	{
		StringFormat(strOutputFile, "/tmp/zsign_temp_%llu.ipa", GetMicroSecond());
	}

	if (!strOutputFile.empty())
	{
		timer.Reset();
		size_t pos = bundle.m_strAppFolder.rfind("/Payload");
		if (string::npos == pos)
		{
			ZLog::Error(">>> Can't Find Payload Directory!\n");
			return -1;
		}

		ZLog::PrintV(">>> Archiving: \t%s ... \n", strOutputFile.c_str());
		string strBaseFolder = bundle.m_strAppFolder.substr(0, pos);
		char szOldFolder[PATH_MAX] = {0};
		if (NULL != getcwd(szOldFolder, PATH_MAX))
		{
			if (0 == chdir(strBaseFolder.c_str()))
			{
				uZipLevel = uZipLevel > 9 ? 9 : uZipLevel;
				RemoveFile(strOutputFile.c_str());
				SystemExec("zip -q -%u -r '%s' Payload", uZipLevel, strOutputFile.c_str());
				chdir(szOldFolder);
				if (!IsFileExists(strOutputFile.c_str()))
				{
					ZLog::Error(">>> Archive Failed!\n");
					return -1;
				}
			}
		}
		timer.PrintResult(true, ">>> Archive OK! (%s)", GetFileSizeString(strOutputFile.c_str()).c_str());
	}

	if (bRet && bInstall)
	{
		SystemExec("ideviceinstaller -i '%s'", strOutputFile.c_str());
	}

	if (0 == strOutputFile.find("/tmp/zsign_tmp_"))
	{
		RemoveFile(strOutputFile.c_str());
	}

	if (0 == strFolder.find("/tmp/zsign_folder_"))
	{
		RemoveFolder(strFolder.c_str());
	}

	gtimer.Print(">>> Done.");
	return bRet ? 0 : -1;
}
