
#include "BinaryWriter.hxx"
#include "VariableManager.hxx"
#include "VtkLoader.hxx"

#include <regex>
#include <limits>

#include <vtkEnSightReader.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkCellData.h>
#include <vtkUnstructuredGrid.h>
#include <vtkInformation.h>

std::string GetPartId(vtkMultiBlockDataSet *data, unsigned partid);
void Extract(vtkMultiBlockDataSet* data, std::vector<std::tuple<std::string, vtkPolyData *>>* blocks);

int main(int argv, char **argc)
{

    if (argv < 2)
    {
        std::cout << "USAGE: EnsightToWF1 <casename>" << std::endl;
        std::cout << "Options: " << std::endl;
        std::cout << "-output - export to specific location" << std::endl;
        exit(1);
    }

    std::string output = std::string(argc[1]);

    for (int i = 1; i < argv; ++i)
    {
        if (strcmp(argc[i], "-output") == 0)
        {
            if (argv < i + 1)
            {
                std::cerr << "Need to provide an output location" << std::endl;
                exit(1);
            }

            output = std::string(argc[i + 1]);
            ++i;
        }
    }

    std::cout << "Finished reading the command line" << std::endl;

    std::cout << "CaseName: " << std::string(argc[1]) << std::endl;

    auto vm = new VariableManager();
    vm->Load(std::string(argc[1]), "variables-surface.json");

    auto vtkLoader = new VtkLoader(vm, false);
    vtkLoader->Setup(std::string(argc[1]), ".*internal.*|.*group.*|.*interface.*|.*INTERFACE.*|.*GP$|.*SYMM.*|.*PRESSURE.*|.*VELOCITY-INLET");
    
    auto reader = vtkLoader->Update();
    auto mb = reader->GetOutputDataObject(0);
    auto mbs = vtkMultiBlockDataSet::SafeDownCast(mb);

    std::vector<std::tuple<std::string, vtkPolyData *>> blocks;
    Extract(mbs, &blocks);

    std::cout << "Writing out file: " << output << ".WF1" << std::endl;
    BinaryWriter out(output, vm);
    out.write(blocks);

    return 0;
}

std::string GetPartId(vtkMultiBlockDataSet *data, unsigned partid)
{
    std::string pid;
    std::stringstream ss;
    auto meta = data->GetMetaData(partid);
    meta->PrintSelf(ss, vtkIndent(0));
    std::smatch m;
    std::string re = "NAME: [A-Za-z0-9-_]+";
    std::string t = ss.str();
    if (std::regex_search(t, m, std::regex(re)))
    {
        auto mt = std::string(m[0]);
        pid = mt.substr(mt.find(":") + 2);
    }

    return pid;
}

void Extract(vtkMultiBlockDataSet* data, std::vector<std::tuple<std::string, vtkPolyData *>>* blocks)
{
    for (int b = 0; b < data->GetNumberOfBlocks(); ++b)
    {
        auto bm = data->GetBlock(b);
        if(bm->GetDataObjectType() == VTK_POLY_DATA)
        {
            // polydata            
            auto pid = GetPartId(data, b);
            auto block = vtkPolyData::SafeDownCast(bm);
            if (block->GetNumberOfPoints() == 0)
            {
                continue;
            }

            blocks->push_back({pid, block});
        }
        else if(bm->GetDataObjectType() == VTK_MULTIBLOCK_DATA_SET)
        {
            // multiblock
            auto block = vtkMultiBlockDataSet::SafeDownCast(bm);
            Extract(block, blocks);
        }
        else 
        {
            std::cout << "encountered an unknown data (" << bm->GetDataObjectType() << ") type in vtk objects" << std::endl;
            exit(1);
        }
    }
}
