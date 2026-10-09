
#include "DedFile.hxx"
#include "Slicer.hxx"
#include "VariableManager.hxx"
#include "Variable.hxx"
#include "Config.hxx"

#include <vtkBox.h>

#include <limits>

DedFileWriter::DedFileWriter(const std::string &fName)
{

    _file = std::make_shared<std::ofstream>(fName.c_str(), std::ios::out | std::ios::binary);

    if (!_file->is_open())
    {
        std::cout << "Problem opening the _file: " << fName << std::endl;
        exit(1);
    }
}

void DedFileWriter::WriteHeader(const std::string &cName,
                                const uint coord, const float start, const float step,
                                const uint numFrames, const Config *conf, vtkBox *domain)
{
    // write out the _file version number!
    uint version = 5;
    _file->write((char *)&version, sizeof(uint));

    // write out the title
    auto caseNameLen = (uint)cName.size();
    _file->write((const char *)&caseNameLen, sizeof(uint));
    _file->write(cName.c_str(), sizeof(char) * caseNameLen);

    // the coord direction
    _file->write((const char *)&coord, sizeof(uint));

    // write the number of slices
    _file->write((const char *)&numFrames, sizeof(uint));

    // write the start position
    _file->write((const char *)&start, sizeof(float));

    // write the step size
    _file->write((const char *)&step, sizeof(float));

    // write out some case information
    _file->write((const char *)&(conf->GetPitch()), sizeof(float));
    _file->write((const char *)&(conf->GetRoll()), sizeof(float));
    _file->write((const char *)&(conf->GetYaw()), sizeof(float));
    _file->write((const char *)&(conf->GetRefVel()), sizeof(float));
    _file->write((const char *)&(conf->GetVelXIn()), sizeof(float));
    _file->write((const char *)&(conf->GetVelYIn()), sizeof(float));
    _file->write((const char *)&(conf->GetVelZIn()), sizeof(float));

    _file->write((const char *)&_compactRange, sizeof(float));

    std::vector<double> mn(3);
    std::vector<double> mx(3);
    domain->GetXMin(&mn[0]);
    domain->GetXMax(&mx[0]);

    // write out the max and mins for the node data
    for (int i = 0; i < 3; ++i)
    {
        if (i != coord)
        {
            const auto minVal = (float)mn[i];
            const auto maxVal = (float)mx[i];

            _minCoord.push_back(minVal);
            _maxCoord.push_back(maxVal);
            _file->write((const char *)&minVal, sizeof(float));
            _file->write((const char *)&maxVal, sizeof(float));
        }
    }
}

void DedFileWriter::WriteSlice(std::shared_ptr<Slicer> slice)
{
    auto vars = slice->GetVariables();
    auto nodes = slice->GetNodes();
    auto data = slice->GetNodeData();
    auto edges = slice->GetEdges();
    auto boundaryNodeEnd = slice->GetEndOfBoundaryNodes();
    auto coord = slice->GetCoord();

    if (_firstSlice)
    {
        auto numVars = (uint)vars.size();

        _file->write((char *)&numVars, sizeof(uint));
        for (uint i = 0; i < numVars; ++i)
        {
            std::string varName = vars[i]->GetMVName();
            const auto varNameSize = (uint)varName.size();
            _file->write((const char *)&varNameSize, sizeof(uint));
            _file->write(varName.c_str(), sizeof(char) * varNameSize);

            const float minAllowable = vars[i]->GetMinBound();
            const float maxAllowable = vars[i]->GetMaxBound();
            _file->write((const char *)&minAllowable, sizeof(float));
            _file->write((const char *)&maxAllowable, sizeof(float));
        }
    }

    _firstSlice = false;

    // write the number of nodes
    const auto numNodes = uint(nodes.size());
    _file->write((const char *)&numNodes, sizeof(uint));

    // need to dump the boundary information
    auto numBoundEdges = uint(edges.size());
    _file->write((const char *)&numBoundEdges, sizeof(uint));
    _file->write((const char *)&boundaryNodeEnd, sizeof(uint));

    // write the nodes and data
    for (uint i = 0; i < numNodes; ++i)
    {
        int counter = 0;
        for (int j = 0; j < 3; ++j)
        {
            if (j != coord)
            {
                if (i < boundaryNodeEnd)
                {
                    auto pos = float(nodes[i][j]);
                    _file->write((char *)&pos, sizeof(float));
                }
                else
                {
                    auto pos = float(nodes[i][j]);
                    WriteFloat(pos, _minCoord[counter], _maxCoord[counter]);
                    counter++;
                }
            }
        }

        if (i >= boundaryNodeEnd)
        {
            for (uint j = 0; j < vars.size(); ++j)
            {
                float value = data[j][i];
                WriteFloat(value, vars[j]->GetMinBound(), vars[j]->GetMaxBound());
            }
        }
    }

    // dump the connectivity
    for (uint i = 0; i < numBoundEdges; ++i)
    {
        uint node1 = edges[i].first;
        uint node2 = edges[i].second;

        _file->write((char *)&node1, sizeof(uint));
        _file->write((char *)&node2, sizeof(uint));
    }
}

short DedFileWriter::CompactFloat(const float var, const float range) const
{
    // stop flow around...
    float temp = var * _compactRange / range;
    temp = std::min(std::max(temp, float(-std::numeric_limits<short>::max())), float(std::numeric_limits<short>::max()));
    return short(temp);
}

void DedFileWriter::WriteFloat(const float var, const float min, const float max) const
{
    const float newVar = std::min(std::max(var, min), max);
    const float range = max - min;
    const float offset = (max + min) * float(0.5);
    short compact = CompactFloat(newVar - offset, range);
    _file->write((char *)&compact, sizeof(short));
}