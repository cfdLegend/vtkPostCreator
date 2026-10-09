
#include "VtkLoader.hxx"
#include "VariableManager.hxx"

#include <vtkPOpenFOAMReader.h>
#include <vtkEnSightReader.h>
#include <vtkDoubleArray.h>

#include <algorithm>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

void VtkLoader::Setup(const std::string& directory)
{
    Setup(directory, ".*group.*|.*interface.*|.*INTERFACE.*");
}

void VtkLoader::Setup(const std::string& directory, const std::string& ignore)
{
    std::cout << "Setting up VTK load" << std::endl;
    std::cout << "Reading Case: " << directory << std::endl;

    if(IsOpenFoam(directory))
    {
        std::cout << "Case is OpenFoam" << std::endl;
        SetupOpenFoam(directory, ignore);
    }
    else 
    {
        std::cout << "Case is Ensight" << std::endl;
        SetupEnsight(directory);
    }
}

vtkAlgorithm* VtkLoader::Update()
{
    if(_time > 0)
    {
        _reader->UpdateTimeStep(_time);
    }
    else 
    {
        _reader->Update();
    }

    return _reader;
}

void VtkLoader::SetupOpenFoam(const std::string& directory, const std::string& ignore)
{
    std::string path = directory + "/foam.foam";

    auto foamReader = vtkPOpenFOAMReader::New();

    foamReader->SetFileName(path.c_str());
    foamReader->SetCaseType(0);
    foamReader->DisableAllPointArrays();
    foamReader->DisableAllLagrangianArrays();
    foamReader->SetCreateCellToPoint(0);
    foamReader->SkipZeroTimeOn();
    foamReader->CacheMeshOff();
    foamReader->ReadZonesOff();
    foamReader->CopyDataToCellZonesOff();

    std::cout << "Parsing meta data from case" << std::endl;
    foamReader->UpdateInformation();

    // make sure reading latest time
    auto times = foamReader->GetTimeValues();
    _time = times->GetMaxNorm();

    for(int i = 0; i < foamReader->GetNumberOfPatchArrays(); ++i)
    {
        auto n = foamReader->GetPatchArrayName(i);

        if(!std::regex_match(n, std::regex(ignore)))
        {
            foamReader->SetPatchArrayStatus(n, 1);
        }
        else 
        {
            foamReader->SetPatchArrayStatus(n, 0);
        }
    }

    auto request = _variables->GetRequestedVariables();
    std::vector<std::string> hasVariables;
    for(int i = 0; i < foamReader->GetNumberOfCellArrays(); ++i)
    {
        auto n = foamReader->GetCellArrayName(i);
        if(std::find(request.begin(), request.end(), n) != request.end())
        {
            hasVariables.emplace_back(n);
        }
        else 
        {
            foamReader->SetCellArrayStatus(n, 0); // dont read
        }
    }

    if(_filter)
    {
        _variables->Filter(hasVariables);
    }
    _reader = foamReader;
}

void VtkLoader::SetupEnsight(const std::string& directory)
{
    fs::path c;
    for (const auto & entry : fs::directory_iterator(directory))
    {
        if(entry.path().extension() == ".case")
        {
            c = entry.path();
            break;
        }
    }

    auto reader = vtkEnSightReader::New();
    reader->SetCaseFileName(c.c_str());

    reader->UpdateInformation();

    auto request = _variables->GetRequestedVariables();
    std::vector<std::string> hasVariables;
    for(int i = 0; i < reader->GetNumberOfCellArrays(); ++i)
    {
        auto n = reader->GetCellArrayName(i);
        if(std::find(request.begin(), request.end(), n) != request.end())
        {
            hasVariables.emplace_back(n);
        }
        else 
        {
            reader->SetCellArrayStatus(n, 0); // dont read
        }
    }

    if(_filter)
    {
        _variables->Filter(hasVariables);
    }
    _reader = reader;
}

bool VtkLoader::IsOpenFoam(const std::string& directory) const
{
    fs::path path = directory + "/foam.foam";
    return fs::exists(path);
}
