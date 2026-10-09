
#include "Slicer.hxx"

#include "TicTok.hxx"
#include "VariableManager.hxx"
#include "Variable.hxx"

extern "C"
{
#include "triangle.h"
}

#include <vtkStaticCleanPolyData.h>
#include <vtkRemoveDuplicatePolys.h>
#include <vtkPointLocator.h>
#include <vtkInformation.h>
#include <vtkFloatArray.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPlane.h>
#include <vtkPlaneCutter.h>

#include <sstream>
#include <queue>
#include <regex>

const auto internal = std::regex(".*internal.*");
const auto interface = std::regex(".*interface.*|.*INTERFACE.*");
const auto name = std::regex("NAME: [A-Za-z0-9-_]+");

bool partIsBoundary(const std::string &partid)
{
    return !std::regex_match(partid, internal);
}

bool IsInterface(const std::string &partid)
{
    return std::regex_match(partid, interface);
}

std::string GetPartId(unsigned partid, vtkMultiBlockDataSet *data)
{
    std::string pid;
    std::stringstream ss;
    auto meta = data->GetMetaData(partid);
    meta->PrintSelf(ss, vtkIndent(0));
    std::smatch m;
    std::string re = "NAME: [A-Za-z0-9-_]+";
    if (auto t = ss.str(); std::regex_search(t, m, name))
    {
        auto mt = std::string(m[0]);
        pid = mt.substr(mt.find(":") + 2);
    }

    return pid;
}

Slicer::Slicer(const std::vector<VariablePtr> &vars, const int c, const float p,
               const bool brakeDuct)
    : _coord(c), _position(p), _variables(vars)
{
    if (brakeDuct)
    {
        _sampleRate.push_back(std::make_pair(0.03, 1)); // threshold and sample rate
        _sampleRate.push_back(std::make_pair(0.003, 2));
        _sampleRate.push_back(std::make_pair(0.0001, 4));
        _sampleRate.push_back(std::make_pair(0.0, 6));
    }
    else
    {
        _sampleRate.push_back(std::make_pair(0.17, 1)); // threshold and sample rate
        _sampleRate.push_back(std::make_pair(0.03, 2));
        _sampleRate.push_back(std::make_pair(0.009, 4));
        _sampleRate.push_back(std::make_pair(0.0, 20));
    }
}

void Slicer::Slice(vtkPlane *plane, vtkPlaneCutter *cutter,
                   std::map<std::string, TicTokPtr, std::less<>> &timers)
{
    timers["Cut"]->Start();

    // do cut
    if (_coord == 0)
    {
        plane->SetOrigin(_position, 0, 0);
    }
    else if (_coord == 1)
    {
        plane->SetOrigin(0, _position, 0);
    }
    else
    {
        plane->SetOrigin(0, 0, _position);
    }
    cutter->Update();

    auto clean = vtkStaticCleanPolyData::New();
    clean->SetInputData(cutter->GetOutput());
    clean->RemoveUnusedPointsOn();
    clean->ConvertLinesToPointsOn();
    clean->Update();

    auto output = vtkMultiBlockDataSet::SafeDownCast(clean->GetOutputDataObject(0));

    timers["Cut"]->Stop();

    timers["MapNodes"]->Start();

    BuildNodeMap(output);
    clean->Delete();

    timers["MapNodes"]->Stop();    

    if (_extractedNodes.size() > 3)
    {
        timers["Triangulate"]->Start();

        TriangulatePoints();

        timers["Triangulate"]->Stop();

        timers["ReduceData"]->Start();

        ReduceData();

        timers["ReduceData"]->Stop();
    }
}

// extract blocks and order them by boundaries first then volume and skip interfaces (boo!)
// multi block data sets can be recursivly multiblock, which is crap and thanks to openfoam is a thing
void Slicer::Extract(vtkMultiBlockDataSet *data, std::vector<std::tuple<std::string, vtkPolyData *>> *blocks)
{
    for (int b = 0; b < data->GetNumberOfBlocks(); ++b)
    {
        auto bm = data->GetBlock(b);
        if (bm->GetDataObjectType() == 0)
        {
            // polydata
            auto pid = GetPartId(b, data);
            auto block = vtkPolyData::SafeDownCast(bm);
            if (block->GetNumberOfPoints() == 0 || IsInterface(pid))
            {
                continue;
            }

            if (partIsBoundary(pid))
            {
                blocks->insert(blocks->begin(), {pid, block});
            }
            else
            {
                blocks->push_back({pid, block});
            }
        }
        else if (bm->GetDataObjectType() == 13)
        {
            // multiblock
            auto block = vtkMultiBlockDataSet::SafeDownCast(bm);
            Extract(block, blocks);
        }
        else
        {
            std::cout << "encountered an unknown data type in vtk objects" << std::endl;
            exit(1);
        }
    }
}

void Slicer::AllocateMemory(const std::vector<std::tuple<std::string, vtkPolyData *>> *blocks)
{
    unsigned ct1 = 0;

    // allocate some memory
    _nodeMap = std::vector<std::vector<unsigned>>(blocks->size(), std::vector<unsigned>());
    for (unsigned b = 0; b < blocks->size(); ++b)
    {
        auto const &[pid, block] = blocks->operator[](b);

        _nodeMap[b].reserve(block->GetNumberOfPoints());

        if (partIsBoundary(pid))
        {
            ct1 += block->GetNumberOfCells();
        }
    }

    _extractedNodeData.resize(_variables.size());
    _extractedEdges.reserve(ct1);
}

void Slicer::BuildNodeMap(vtkMultiBlockDataSet *output)
{
    // foreach block
    // foreach point
    // check if point is in any previous blocks, if it is skip
    // if new, get point and get data
    // do boundaries first

    // order the blocks by boundaries first then volume and skip interfaces (boo!)
    std::vector<std::tuple<std::string, vtkPolyData *>> blocks;
    Extract(output, &blocks);
    AllocateMemory(&blocks);

    std::vector<vtkPointLocator *> trees; // for each block make a tree

    // begin point extraction
    for (unsigned b = 0; b < blocks.size(); ++b)
    {
        auto const &[pid, block] = blocks[b];

        // add tree of the current block to trees
        auto tree = vtkPointLocator::New();
        tree->SetDataSet(block);
        tree->Update();
        trees.push_back(tree);

        for (unsigned p = 0; p < block->GetNumberOfPoints(); ++p)
        {
            bool newNode = AddNodeToMap(block, b, p, &trees);

            if (newNode)
            {
                // after scanning all trees this node hasnt been used before
                // so map it and extract its data
                ExtractNode(block, b, p);
            }
        }

        // nodes have been processed, now get the edges if its a boundary
        if (partIsBoundary(pid))
        {
            ExtractBoundary(block, b);
        }
    }

    std::set<unsigned> skipList;
    unsigned c = 0;
    for (unsigned i = 0; i < _extractedNodes.size(); ++i)
    {
        if ((i < _boundaryNodes.size() && _boundaryNodes.find(i) == _boundaryNodes.end()))
        {
            skipList.insert(i);
        }
    }

    auto blowEdgesAwayOnError = false;

    if (!skipList.empty())
    {
        std::cout << std::endl
                  << _position << " "
                  << skipList.size() << std::endl;

        for (auto s : skipList)
        {
            std::cout << s << std::endl;
        }

        if(blowEdgesAwayOnError)
        {
            _boundaryNodes.clear();
            _extractedEdges.clear();
        }
        else 
        {
            RepairBoundary(skipList);
        }
    }

    for (unsigned t = 0; t < trees.size(); ++t)
    {
        trees[t]->Delete();
    }
    trees.clear();
}

bool Slicer::AddNodeToMap(vtkPolyData *block, unsigned b, unsigned p, std::vector<vtkPointLocator *> *trees)
{
    bool newNode = true;
    std::vector<double> pt(3);
    auto list = vtkIdList::New();

    // see if node is in any trees
    block->GetPoint(p, &pt[0]);

    for (unsigned t = 0; t < trees->size(); ++t)
    {
        trees->at(t)->FindPointsWithinRadius(1e-6, &pt[0], list);
        list->Sort();

        // found something
        if (list->GetNumberOfIds() > 0)
        {
            // the lowest ID is itself (i.e a new point)
            if (t == b && list->GetId(0) == p)
            {
                continue;
            }

            // an idiot check
            if (t == b && list->GetId(0) > p)
            {
                std::cout << "this doesnt make sense" << std::endl;
            }

            newNode = false;
            _nodeMap[b].push_back(_nodeMap[t][list->GetId(0)]);
            break;
        }
    }

    return newNode;
}

void Slicer::ExtractNode(vtkPolyData *block, unsigned b, unsigned p)
{
    auto data = block->GetPointData();
    auto newId = _extractedNodes.size();
    std::vector<double> cf(3);
    block->GetPoint(p, &cf[0]);
    _extractedNodes.push_back(cf);
    _nodeMap[b].push_back(newId);

    for (unsigned v = 0; v < _variables.size(); ++v)
    {
        _extractedNodeData[v].push_back(_variables[v]->ExtractData(data, p));
    }
}

void Slicer::ExtractBoundary(vtkPolyData* block, unsigned b)
{
    for (unsigned c = 0; c < block->GetNumberOfCells(); ++c)
    {
        auto cell = block->GetCell(c);

        if (cell->GetNumberOfPoints() != 2)
        {
            continue;
        }

        auto v1 = cell->GetPointId(0);
        auto v2 = cell->GetPointId(1);

        auto lv1 = _nodeMap[b][v1];
        auto lv2 = _nodeMap[b][v2];

        if (lv1 == lv2)
        {
            continue;
        }

        _boundaryNodes.insert(lv1);
        _boundaryNodes.insert(lv2);
        _extractedEdges.push_back(std::make_pair(lv1, lv2));
    }
}

void Slicer::RepairBoundary(const std::set<unsigned> &skipList)
{
    unsigned ct = 0;
    std::vector<unsigned> whereDidNodeEndUp(_extractedNodes.size());
    for (int i = 0; i < _extractedNodes.size(); ++i)
    {
        if (skipList.find(i) == skipList.end())
        {
            whereDidNodeEndUp[i] = ct++;
        }
        else
        {
            whereDidNodeEndUp[i] = ct;
        }
    }

    for (int i = 0; i < whereDidNodeEndUp.size(); ++i)
    {
        _extractedNodes[whereDidNodeEndUp[i]] = _extractedNodes[i];
        for (auto j = _extractedNodeData.begin(); j != _extractedNodeData.end(); ++j)
        {
            if ((*j).size() != 0)
            {
                (*j)[whereDidNodeEndUp[i]] = (*j)[i];
            }
        }
    }

    _extractedNodes.resize(whereDidNodeEndUp.size() - skipList.size());
    for (auto j = _extractedNodeData.begin(); j != _extractedNodeData.end(); ++j)
    {
        if ((*j).size() != 0)
        {
            (*j).resize(whereDidNodeEndUp.size() - skipList.size());
        }
    }

    for (int i = 0; i < _extractedEdges.size(); ++i)
    {
        _extractedEdges[i].first = whereDidNodeEndUp[_extractedEdges[i].first];
        _extractedEdges[i].second = whereDidNodeEndUp[_extractedEdges[i].second];
    }
}

void Slicer::TriangulatePoints()
{
    // triangulate the node set
    // this is going to be fairly nasty!  use triangle to generate the triangulation

    // triangle is compiled in double mode
    std::vector<double> nodesForTriangle(_extractedNodes.size() * 2);
    int counter = 0;
    for (uint i = 0; i < _extractedNodes.size(); ++i)
    {
        for (int j = 0; j < 3; ++j)
            if (j != _coord)
            {
                nodesForTriangle[counter++] = _extractedNodes[i][j];
            }
    }

    counter = 0;
    std::vector<int> edgesForTriangle(_extractedEdges.size() * 2);
    for (auto it : _extractedEdges)
    {
        edgesForTriangle[counter * 2 + 0] = it.first;
        edgesForTriangle[counter * 2 + 1] = it.second;
        counter++;
    }

    triangulateio *in = new triangulateio;
    in->numberofpoints = _extractedNodes.size();
    in->numberofpointattributes = 0;
    in->numberoftriangles = 0;
    in->numberofcorners = 0;
    in->numberoftriangleattributes = 0;
    in->numberofsegments = _extractedEdges.size();
    in->numberofholes = 0;
    in->numberofregions = 0;
    in->numberofedges = 0;

    in->pointlist = &nodesForTriangle[0];
    in->pointattributelist = NULL;
    in->pointmarkerlist = NULL;
    in->trianglelist = NULL;
    in->triangleattributelist = NULL;
    in->trianglearealist = NULL;
    in->segmentlist = &edgesForTriangle[0];
    in->segmentmarkerlist = NULL;
    in->holelist = NULL;
    in->regionlist = NULL;
    in->edgelist = NULL;
    in->edgemarkerlist = NULL;
    in->normlist = NULL;

    _triangeOutput = new triangulateio;
    _triangeOutput->numberofpoints = 0;
    _triangeOutput->numberofpointattributes = 0;
    _triangeOutput->numberoftriangles = 0;
    _triangeOutput->numberofcorners = 0;
    _triangeOutput->numberoftriangleattributes = 0;
    _triangeOutput->numberofsegments = 0;
    _triangeOutput->numberofholes = 0;
    _triangeOutput->numberofregions = 0;
    _triangeOutput->numberofedges = 0;

    _triangeOutput->pointlist = NULL;
    _triangeOutput->pointattributelist = NULL;
    _triangeOutput->pointmarkerlist = NULL;
    _triangeOutput->trianglelist = NULL;
    _triangeOutput->triangleattributelist = NULL;
    _triangeOutput->trianglearealist = NULL;
    _triangeOutput->segmentlist = NULL;
    _triangeOutput->segmentmarkerlist = NULL;
    _triangeOutput->holelist = NULL;
    _triangeOutput->regionlist = NULL;
    _triangeOutput->edgelist = NULL;
    _triangeOutput->edgemarkerlist = NULL;
    _triangeOutput->normlist = NULL;

    // c = convex hull
    // z = indicies from zero rather than 1
    // p = stops filling in between boundaries
    // Y = no boundary splitting
    // Q = no screen output
    triangulate((char *)"-czpYYQS", in, _triangeOutput, (struct triangulateio *)NULL);

    _extractedNodePriority = std::vector<float>(in->numberofpoints, 2.0);

    for (int i = in->numberofpoints; i < _triangeOutput->numberofpoints; ++i)
    {
        _extractedNodes.push_back(std::vector<double>(3, _position));
        int counter = 0;
        for (int j = 0; j < 3; ++j)
        {
            if (j != _coord)
            {
                _extractedNodes.back()[j] = _triangeOutput->pointlist[i * 2 + counter];
                counter++;
            }
        }

        for (uint j = 0; j < _variables.size(); ++j)
            _extractedNodeData[j].push_back(0);

        _extractedNodePriority.push_back(0.0);
    }

    in->pointlist = nullptr; // will be deleted by vector destructor
    in->segmentlist = nullptr;
    delete in;
}

void Slicer::Cleanup()
{
    if (_triangeOutput != 0)
    {
        free(_triangeOutput->pointlist);
        free(_triangeOutput->pointattributelist);
        free(_triangeOutput->pointmarkerlist);
        free(_triangeOutput->trianglelist);
        free(_triangeOutput->triangleattributelist);
        free(_triangeOutput->trianglearealist);
        free(_triangeOutput->segmentlist);
        free(_triangeOutput->segmentmarkerlist);
        free(_triangeOutput->holelist);
        free(_triangeOutput->regionlist);
        free(_triangeOutput->edgelist);
        free(_triangeOutput->edgemarkerlist);
        free(_triangeOutput->normlist);

        delete _triangeOutput;
    }
}

struct CompareFirst
{
    bool operator()(const std::pair<float, int> &a, const std::pair<float, int> &b) const
    {
        return a.first > b.first;
    }
};

void Slicer::ReduceData()
{
    std::vector<unsigned> useForDataReduction;
    for (unsigned i = 0; i < _variables.size(); ++i)
    {
        if (_variables[i]->UseForReduction())
        {
            useForDataReduction.push_back(i);
        }
    }

    // calculate the priority for each node in the solution based on
    // 1) the region they are in.. (this is already added in the insertFieldPoints function)
    // 2) the solution gradient of the node (keep max and min nodes)

    float maxMaxDiff = 0.0;
    float minMaxDiff = 1e6;
    std::vector<float> gradientNodePriority(_extractedNodes.size(), 0.0);

    auto boundaryNodeEnd = _boundaryNodes.size();

    std::vector<float> nodeGrads;
    std::vector<int> nodeConnCounter;
    // for each variable calculate the gradients over a triangle and add this to the nodes gradient
    for (uint i = 0; i < useForDataReduction.size(); ++i)
    {
        auto vid = useForDataReduction[i];
        nodeGrads.assign(_extractedNodes.size(), 0.0);
        nodeConnCounter.assign(_extractedNodes.size(), 0);

        for (uint j = 0; j < _triangeOutput->numberoftriangles; ++j)
        {
            int node0 = _triangeOutput->trianglelist[j * 3 + 0];
            int node1 = _triangeOutput->trianglelist[j * 3 + 1];
            int node2 = _triangeOutput->trianglelist[j * 3 + 2];

            auto node0Val = _extractedNodeData[vid][node0];
            auto node1Val = _extractedNodeData[vid][node1];
            auto node2Val = _extractedNodeData[vid][node2];

            auto minVal = std::min(node0Val, std::min(node1Val, node2Val));
            auto maxVal = std::max(node0Val, std::max(node1Val, node2Val));

            auto minValScaled = (minVal - _variables[vid]->GetMinBound()) / (_variables[vid]->GetMaxBound() - _variables[vid]->GetMinBound());
            auto maxValScaled = (maxVal - _variables[vid]->GetMinBound()) / (_variables[vid]->GetMaxBound() - _variables[vid]->GetMinBound());

            auto diff = std::abs(maxValScaled - minValScaled);
            nodeGrads[node0] += diff;
            ++nodeConnCounter[node0];
            nodeGrads[node1] += diff;
            ++nodeConnCounter[node1];
            nodeGrads[node2] += diff;
            ++nodeConnCounter[node2];
        }

        float tempMaxDiff = 0.0;
        for (uint j = boundaryNodeEnd; j < _extractedNodes.size(); ++j)
        {
            nodeGrads[j] /= float(nodeConnCounter[j]);
            tempMaxDiff = std::max(tempMaxDiff, nodeGrads[j]);
            gradientNodePriority[j - boundaryNodeEnd] = std::max(gradientNodePriority[j - boundaryNodeEnd], nodeGrads[j]);
        }

        maxMaxDiff = std::max(maxMaxDiff, tempMaxDiff);
        minMaxDiff = std::min(minMaxDiff, tempMaxDiff);
    }

    auto maxDiff = std::min(minMaxDiff, (float)0.1) * 0.75;

    std::vector<bool> deleteNode(_extractedNodes.size(), true);
    for (int i = 0; i < _boundaryNodes.size(); ++i) // keep the boundary nodes...
    {
        deleteNode[i] = false;
    }

    std::vector<std::pair<float, int>> orderedNodes;
    for (uint i = boundaryNodeEnd; i < _extractedNodes.size(); ++i)
    {
        orderedNodes.push_back(std::make_pair(_extractedNodePriority[i - boundaryNodeEnd] * gradientNodePriority[i - boundaryNodeEnd], i));
    }

    CompareFirst compare;
    std::sort(orderedNodes.begin(), orderedNodes.end(), compare);

    int counter = 0;
    int intCounter = 0;

    while (counter < int(orderedNodes.size()))
    {
        int add = _sampleRate[intCounter].second;
        int node = orderedNodes[counter].second;
        if (orderedNodes[counter].first < _sampleRate[intCounter].first * maxDiff)
        {
            ++intCounter;
        }

        deleteNode[node] = false;
        counter += add;
    }

    // now delete the nodes we do not need...

    DeleteNodes(deleteNode);
}

void Slicer::DeleteNodes(const std::vector<bool> &deleteNode)
{
    // now delete the nodes we do not need...
    int counter = 0;
    for (uint i = 0; i < deleteNode.size(); ++i)
    {
        if (!deleteNode[i])
        {
            _extractedNodes[counter] = _extractedNodes[i];
            for (auto j = _extractedNodeData.begin(); j != _extractedNodeData.end(); ++j)
            {
                if ((*j).size() != 0)
                {
                    (*j)[counter] = (*j)[i];
                }
            }

            counter++;
        }
    }

    _extractedNodes.resize(counter);
    for (auto j = _extractedNodeData.begin(); j != _extractedNodeData.end(); ++j)
    {
        if ((*j).size() != 0)
        {
            (*j).resize(counter);
        }
    }
}
