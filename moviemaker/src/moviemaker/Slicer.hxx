#ifndef SLICER_HXX
#define SLICER_HXX

#include <unordered_set>
#include <set>
#include <vector>
#include <map>
#include <memory>

class Variable;
using VariablePtr = std::shared_ptr<Variable>;
class TicTok;
using TicTokPtr = std::shared_ptr<TicTok>;

class vtkPlane;
class vtkPolyData;
class vtkPointLocator;
class vtkPlaneCutter;
class vtkMultiBlockDataSet;

class triangulateio;

class Slicer
{
public:

    Slicer(const std::vector<VariablePtr> &vars, const int c, const float p,
        const bool brakeDuct);

    void Slice(vtkPlane* plane, vtkPlaneCutter* cutter, 
                std::map<std::string, TicTokPtr, std::less<>>& timers);

    const std::vector<VariablePtr>& GetVariables() const
    {
        return _variables;
    }

    const std::vector<std::vector<double>>& GetNodes() const
    {
        return _extractedNodes;
    }

    const std::vector<std::vector<float>>& GetNodeData() const
    {
        return _extractedNodeData;
    }

    const std::vector<std::pair<unsigned, unsigned>>& GetEdges() const
    {
        return _extractedEdges;
    }

    unsigned GetEndOfBoundaryNodes() const
    {
        return (unsigned)_boundaryNodes.size();
    }

    unsigned GetCoord() const
    {
        return _coord;
    }

private:

    void BuildNodeMap(vtkMultiBlockDataSet* output);
    bool AddNodeToMap(vtkPolyData* block, unsigned b, unsigned p, std::vector<vtkPointLocator *> *trees);
    void ExtractNode(vtkPolyData* block, unsigned b, unsigned p);
    void ExtractBoundary(vtkPolyData* block, unsigned b);
    void Extract(vtkMultiBlockDataSet *data, std::vector<std::tuple<std::string, vtkPolyData *>> *blocks);
    void AllocateMemory(const std::vector<std::tuple<std::string, vtkPolyData *>> *blocks);
    void RepairBoundary(const std::set<unsigned>& skipList);

    void TriangulatePoints();

    void Cleanup();

    void ReduceData();
    void DeleteNodes(const std::vector<bool> &deleteNode);

    // slice information
    const int _coord;
    const float _position;

    triangulateio* _triangeOutput;

    std::vector<std::vector<unsigned>> _nodeMap;
    std::vector<std::vector<double>> _extractedNodes;
    std::vector<std::vector<float>> _extractedNodeData;
    std::vector<float> _extractedNodePriority;
    std::vector<std::pair<unsigned, unsigned>> _extractedEdges;
    std::unordered_set<unsigned> _boundaryNodes;

    std::vector<std::pair<float, int>> _sampleRate;

    const std::vector<VariablePtr> _variables;
};

#endif
