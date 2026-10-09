
#include "Config.hxx"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <string.h>

#include <filesystem>
#include <array>
#include <algorithm>
namespace fs = std::filesystem;


void Config::ReadGlobalSettings(
    const std::string& caseDir, 
    const std::string& configType, 
    const std::string& fcsuffix)
{
    auto config = fs::path(caseDir + "/moviemaker_" + configType + ".cnf");

    std::cout << std::endl;
    std::cout << "Checking first priority config file: " << config << std::endl;

    if (fs::exists(config))
    {
        std::cout << "Custom config file found" << std::endl;
    }
    else
    {
        std::string movieMakerConfigDir = "/cfd/aerocfd/conf/postpro/moviemaker/";
        config = fs::path(movieMakerConfigDir + "moviemaker_" + configType + fcsuffix + ".cnf");

        std::cout << "Checking second priority config file: " << config << std::endl;
        if (fs::exists(config))
        {
            std::cout << "Config file found" << std::endl;
        }
        else
        {
            std::cout << "ERROR: could find a valid config file" << std::endl;
            exit(1);
        }
    }

    _sliceOptions = std::vector<SliceOptions>(3);

    std::ifstream configFile(std::string(config).c_str(), std::ios::in);

    if (configFile.is_open())
    {
        std::cout << "Reading in the config file: " << config << std::endl;
        std::string line;

        while (!configFile.eof())
        {
            std::getline(configFile, line);
            if (configFile.eof())
                break;

            std::istringstream str(line.c_str());
            std::string varName;

            str >> varName;

            if (strcmp(varName.c_str(), std::string("xSliceStart").c_str()) == 0)
            {
                str >> varName; // read the "=" sign
                str >> _sliceOptions[0].Start;
            }
            else if (strcmp(varName.c_str(), std::string("xSliceEnd").c_str()) == 0)
            {
                str >> varName; // read the "=" sign
                str >> _sliceOptions[0].End;
            }
            else if (strcmp(varName.c_str(), std::string("xSliceSplit").c_str()) == 0)
            {
                str >> varName; // read the "=" sign
                str >> _sliceOptions[0].Split;
            }
            else if (strcmp(varName.c_str(), std::string("ySliceStart").c_str()) == 0)
            {
                str >> varName; // read the "=" sign
                str >> _sliceOptions[1].Start;
            }
            else if (strcmp(varName.c_str(), std::string("ySliceEnd").c_str()) == 0)
            {
                str >> varName; // read the "=" sign
                str >> _sliceOptions[1].End;
            }
            else if (strcmp(varName.c_str(), std::string("ySliceSplit").c_str()) == 0)
            {
                str >> varName; // read the "=" sign
                str >> _sliceOptions[1].Split;
            }
            else if (strcmp(varName.c_str(), std::string("zSliceStart").c_str()) == 0)
            {
                str >> varName; // read the "=" sign
                str >> _sliceOptions[2].Start;
            }
            else if (strcmp(varName.c_str(), std::string("zSliceEnd").c_str()) == 0)
            {
                str >> varName; // read the "=" sign
                str >> _sliceOptions[2].End;
            }
            else if (strcmp(varName.c_str(), std::string("zSliceSplit").c_str()) == 0)
            {
                str >> varName; // read the "=" sign
                str >> _sliceOptions[2].Split;
            }
        }
    }
}

void Config::ReadDediSettings(const std::string& caseDir)
{
    fs::path c;
    for (const auto & entry : fs::directory_iterator(caseDir))
    {
        if(entry.path().extension() == ".dedi")
        {
            c = entry.path();
            break;
        }
    }

    std::ifstream caseData(c.c_str(), std::ios::in);

    if (caseData.is_open())
    {
        std::string line;

        bool readVelocityX = false;
        bool readVelocityY = false;
        bool readVelocityZ = false;

        _refVel = 67.0;

        while (!caseData.eof())
        {
            std::getline(caseData, line);
            if (caseData.eof())
                break;

            std::istringstream str(line.c_str());
            std::string varName;
            str >> varName;
            if (strcmp(varName.c_str(), "pitch") == 0)
            {
                str >> varName; // read the "=" sign
                str >> _pitch;
            }
            else if (strcmp(varName.c_str(), "roll") == 0)
            {
                str >> varName; // read the "=" sign
                str >> _roll;
            }
            else if (strcmp(varName.c_str(), "yaw") == 0)
            {
                str >> varName; // read the "=" sign
                str >> _yaw;
            }
            else if (strcmp(varName.c_str(), "refVel") == 0)
            {
                str >> varName; // read the "=" sign
                str >> _refVel;
            }
            else if (strcmp(varName.c_str(), "velxin") == 0)
            {
                str >> varName; // read the "=" sign
                str >> _velXin;
                readVelocityX = true;
            }
            else if (strcmp(varName.c_str(), "velyin") == 0)
            {
                str >> varName; // read the "=" sign
                str >> _velYin;
                readVelocityY = true;
            }
            else if (strcmp(varName.c_str(), "velzin") == 0)
            {
                str >> varName; // read the "=" sign
                str >> _velZin;
                readVelocityZ = true;
            }
        }

        std::cout << "Setting the case pitch: " << _pitch << ", roll: " << _roll << ", yaw: " << _yaw << ", and reference velocity to: " << _refVel << std::endl;

        std::array<float,3> gpVector = {std::cos(_pitch), 0.0, std::sin(_pitch)};
        float lenGpVector = std::sqrt(gpVector[0]*gpVector[0] + gpVector[1]*gpVector[1] + gpVector[2]*gpVector[2]);

        std::for_each(gpVector.begin(), gpVector.end(), [&] (auto & comp) { comp /= lenGpVector; } );

        if (!readVelocityX)
        {
            _velXin = gpVector[0];
        }
        else
        {
            _velXin /= _refVel;
        }

        if (!readVelocityY)
        {
            _velYin = gpVector[1];
        }
        else
        {
            _velYin /= _refVel;
        }

        if (!readVelocityZ)
        {
            _velZin = gpVector[2];
        }
        else
        {
            _velZin /= _refVel;        
        }

        _velXin = 1.0;
        _velYin = 0.0;
        _velZin = 0.0;
    }
    else 
    {
        std::cout << "Failed to open dedi" << std::endl;
        exit(1);
    }
}
