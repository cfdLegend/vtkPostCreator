
#include <iostream>
#include <iomanip>

#include "BinaryWriter.hxx"
#include "Variable.hxx"
#include "VariableManager.hxx"

#include <vtkCellDataToPointData.h>
#include <vtkPolyDataNormals.h>
#include <vtkTriangleFilter.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkGeometryFilter.h>
#include <vtkInformation.h>
#include <vtkStripper.h>
#include <vtkStaticCleanPolyData.h>

#include <sstream>
#include <filesystem>
namespace fs = std::filesystem;

BinaryWriter::BinaryWriter(const std::string &bName, const VariableManager *vm)
    : _vars(vm->GetVariables(0)), _baseName(bName)
{
    _out = std::make_shared<std::ofstream>(
        std::string(_baseName.c_str() + std::string(".WF1")).c_str(),
        std::ios::out | std::ios::binary);
}

void BinaryWriter::setup(const std::vector<std::tuple<std::string, vtkPolyData *>> &blocks)
{
    std::cout << "Setting up node mapping and interpolating data to nodes" << std::endl;

    for (const auto &[pid, block] : blocks)
    {
        auto clean = vtkStaticCleanPolyData::New();
        clean->SetInputData(block);
        clean->RemoveUnusedPointsOn();
        clean->ConvertLinesToPointsOff();
        clean->Update();

        auto cellToNode = vtkCellDataToPointData::New();
        cellToNode->SetInputData(clean->GetOutput());
        cellToNode->ProcessAllArraysOn();
        cellToNode->PassCellDataOff();
        cellToNode->Update();

        auto triangles = vtkTriangleFilter::New();
        triangles->SetInputData(cellToNode->GetPolyDataOutput());
        triangles->Update();

        auto normals = vtkPolyDataNormals::New();
        normals->SetComputeCellNormals(false);
        normals->SetComputePointNormals(true);
        normals->ConsistencyOn();
        normals->SetInputData(triangles->GetOutput());
        normals->Update();

        auto stripper = vtkStripper::New();
        stripper->SetInputData(normals->GetOutput());
        stripper->Update();

        _parts.emplace_back(std::tuple(pid, stripper->GetOutput()));
    }

    _nodeOffsets = std::vector<long long>(1, 0);
    std::cout << "Parts Size: " << _parts.size() << std::endl;
    faceCounter = 0;
    for (const auto &[pid, part] : _parts)
    {
        _nodeOffsets.push_back(_nodeOffsets.back() + part->GetNumberOfPoints());
        for (int i = 0; i < part->GetNumberOfCells(); ++i)
        {
            faceCounter += part->GetCell(i)->GetNumberOfPoints() - 2;
        }
    }
    std::cout << "Num Nodes: " << _nodeOffsets.back() << std::endl;
}

int BinaryWriter::setIntAccuracy(const std::vector<int> &faceNodes)
{
    int min = std::numeric_limits<int>::max();
    int max = std::numeric_limits<int>::min();

    for (uint i = 0; i < faceNodes.size(); ++i)
    {
        min = std::min(faceNodes[i], min);
        max = std::max(faceNodes[i], max);
    }

    return setIntAccuracy(min, max);
}

int BinaryWriter::setIntAccuracy(const int min_, const int max_)
{

    _intAccuracy = 32;
    int diff = max_ - min_;

    // use diff to set the int accuracy...
    std::vector<int> options;
    options.push_back(0);
    options.push_back(8);
    options.push_back(16);
    options.push_back(32);

    for (uint i = 1; i < options.size(); ++i)
    {
        if (diff > std::pow(double(2), int(options[i - 1])) && diff < std::pow(double(2), int(options[i])))
        {
            _intAccuracy = options[i];
            break;
        }
    }

    return min_;
}

void BinaryWriter::write(const std::vector<std::tuple<std::string, vtkPolyData *>> &blocks)
{
    setup(blocks);

    // file format
    long long numNodes = _nodeOffsets.back();

    // dump the file size...
    int fileSize = 0;
    fileSize += numNodes * 3 * sizeof(short);
    fileSize += faceCounter * 4 * sizeof(int); // rough num face nodes...
    fileSize += faceCounter * 4 * sizeof(short);

    // do we have temperature
    auto version = _vars.size() == 6 ? 108 : 107;

    _out->write((char *)&version, sizeof(int));
    std::cout << "Version = " << version << std::endl;

    // write _out the approx file size...
    _out->write((char *)&fileSize, sizeof(int));

    // name
    std::string caseName = ((fs::path)_baseName).stem();
    auto const sLen = caseName.length();
    _out->write((const char *)&sLen, sizeof(int));
    _out->write(caseName.c_str(), sizeof(char) * int(sLen));

    _out->write((const char *)&_compactRange, sizeof(int));

    // num nodes
    _out->write((char *)&numNodes, sizeof(int));

    std::vector<float> coordMin(3, std::numeric_limits<float>::max());
    std::vector<float> coordMax(3, -std::numeric_limits<float>::max());

    for (const auto &[pid, part] : _parts)
    {
        std::vector<double> coords(3);
        for (int i = 0; i < part->GetNumberOfPoints(); ++i)
        {
            part->GetPoint(i, &coords[0]);
            for (int j = 0; j < 3; ++j)
            {
                coordMin[j] = std::min(coordMin[j], float(coords[j]));
                coordMax[j] = std::max(coordMax[j], float(coords[j]));
            }
        }
    }

    for (int j = 0; j < 3; ++j)
    {
        coordMin[j] = int(coordMin[j] - 1.0);
        coordMax[j] = int(coordMax[j] + 1.0);

        const int minVal = coordMin[j];
        const int maxVal = coordMax[j];

        _out->write((const char *)&minVal, sizeof(int));
        _out->write((const char *)&maxVal, sizeof(int));
    }

    // allowdelta
    auto allowDelta = 1;
    _out->write((const char *)&allowDelta, sizeof(int));

    for (const auto &var : _vars)
    {
        const auto mn = int(var->GetMinBound());
        const auto mx = int(var->GetMaxBound());
        _out->write((const char *)&mn, sizeof(int));
        _out->write((const char *)&mx, sizeof(int));
    }

    // write all nodes and data
    for (const auto &[pid, part] : _parts)
    {
        auto data = part->GetPointData();

        std::vector<double> coords(3);

        for (int i = 0; i < part->GetNumberOfPoints(); ++i)
        {
            part->GetPoint(i, &coords[0]);
            for (int j = 0; j < 3; ++j)
            {
                float var = float(coords[j]);
                writeFloat(var, coordMin[j], coordMax[j]);
            }

            for (const auto &var : _vars)
            {
                auto vd = var->ExtractDataVector(data, i);
                for (const auto &d : vd)
                {
                    writeFloat(d, var->GetMinBound(), var->GetMaxBound());
                }
            }
        }
    }

    // write faces

    const auto numParts = _parts.size();
    _out->write((const char *)&numParts, sizeof(int));

    int p = 0;
    for (const auto &[pid, part] : _parts)
    {
        const auto partNameLength = pid.size();
        _out->write((const char *)&partNameLength, sizeof(int));
        _out->write(pid.c_str(), sizeof(char) * int(partNameLength));

        std::vector<int> triList;
        std::vector<int> triListStrip;

        for (unsigned c = 0; c < part->GetNumberOfCells(); ++c)
        {
            auto cell = part->GetCell(c);
            auto verts = cell->GetPointIds();

            if (verts->GetNumberOfIds() == 3)
            {
                triList.push_back(verts->GetId(0) + _nodeOffsets[p]);
                triList.push_back(verts->GetId(1) + _nodeOffsets[p]);
                triList.push_back(verts->GetId(2) + _nodeOffsets[p]);
            }
            else // is a strip with multiple triangles 
            {
                if(triListStrip.size() != 0)
                {
                    triListStrip.push_back(triListStrip.back());
                    triListStrip.push_back(verts->GetId(0) + _nodeOffsets[p]);
                }

                for(int v = 0; v < verts->GetNumberOfIds(); ++v)
                {
                    triListStrip.push_back(verts->GetId(v) + _nodeOffsets[p]);
                }
            }
        }
        p++;

        int offset = setIntAccuracy(triList);
        _out->write((char *)&offset, sizeof(int));
        _out->write((char *)&_intAccuracy, sizeof(int));

        auto triSizeInfo = triList.size();
        _out->write((const char *)&triSizeInfo, sizeof(int));
        for (unsigned int i = 0; i < triList.size(); i++)
        {
            const auto node = uint(triList[i] - offset);
            writeInt(node);
        }

        offset = setIntAccuracy(triListStrip);
        _out->write((char *)&offset, sizeof(int));
        _out->write((char *)&_intAccuracy, sizeof(int));

        triSizeInfo = triListStrip.size();
        _out->write((const char *)&triSizeInfo, sizeof(int));
        for (unsigned int i = 0; i < triListStrip.size(); i++)
        {
            const auto node = uint(triListStrip[i] - offset);
            writeInt(node);
        }
    }

    // write an extra character for safety...
    _out->write(" ", 1);
}
