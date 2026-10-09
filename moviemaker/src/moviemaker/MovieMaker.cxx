
#include "MovieMaker.hxx"

#include "Slicer.hxx"
#include "DedFile.hxx"
#include "VariableManager.hxx"
#include "Config.hxx"
#include "VtkLoader.hxx"
#include <vtkMultiBlockDataSet.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkCellDataToPointData.h>
#include <vtkInformation.h>
#include <vtkExtractGeometry.h>
#include <vtkPlane.h>
#include <vtkPlaneCutter.h>
#include <vtkBox.h>
#include <vtkStaticCleanUnstructuredGrid.h>

#include <queue>
#include <string>
#include <sstream>
#include <regex>
#include <omp.h>

MovieMaker::MovieMaker(
    Config *conf,
    const bool fc,
    const bool bd,
    const std::string &out,
    const std::string &cName,
    const std::string &filePrefix)
    : _caseName(cName), _conf(conf), _filePrefix(filePrefix), _outputDir(out), _fullCar(fc), _brakeDuct(bd)
{
    _boxOfInterest = vtkBox::New();
    _boxOfInterest->SetXMin(-1.8, -1.8, -1.0);
    _boxOfInterest->SetXMax(4.8, 1.8, 2.0);
}

void MovieMaker::MakeMovies(VtkLoader* loader, VariableManager* vm)
{
    auto vtkLoad = TicTok();
    vtkLoad.Start();

    auto reader = loader->Update();

    vtkLoad.Stop();
    vtkLoad.Report("VTK load");

    std::cout << "About to make movies..." << std::endl;
    _sliceTimers["Cut"] = std::make_shared<TicTok>();
    _sliceTimers["MapNodes"] = std::make_shared<TicTok>();
    _sliceTimers["Triangulate"] = std::make_shared<TicTok>();
    _sliceTimers["ReduceData"] = std::make_shared<TicTok>();

    TicTok setup;
    setup.Start();

    // do a global subset of the domain of interest
    // do cell to node here for all directions
    auto clipper = vtkSmartPointer<vtkExtractGeometry>::New();
    clipper->SetImplicitFunction(_boxOfInterest);
    clipper->SetInputData(reader->GetOutputDataObject(0));
    clipper->ExtractBoundaryCellsOn();
    clipper->Update();

    auto clean = vtkSmartPointer<vtkStaticCleanUnstructuredGrid>::New();
    clean->SetInputDataObject(clipper->GetOutputDataObject(0));
    clean->Update();

    auto cellToNode = vtkSmartPointer<vtkCellDataToPointData>::New();
    cellToNode->SetInputData(clean->GetOutputDataObject(0));
    cellToNode->PassCellDataOff();
    cellToNode->ProcessAllArraysOn();
    cellToNode->Update();

    auto domainOfInterest = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    domainOfInterest->DeepCopy(cellToNode->GetOutputDataObject(0));

    setup.Stop();

    TicTok movie;
    movie.Start();
    CreateMovie(0, _conf->GetSliceOption(0), vm->GetVariables(0), "XMovieSlices.ded", domainOfInterest);
    CreateMovie(1, _conf->GetSliceOption(1), vm->GetVariables(1), "YMovieSlices.ded", domainOfInterest);
    CreateMovie(2, _conf->GetSliceOption(2), vm->GetVariables(2), "ZMovieSlices.ded", domainOfInterest);

    movie.Stop();

    std::cout << std::endl
              << std::endl;

    std::cout << "Timings for single exectution blocks" << std::endl;
    _dedfilewrite.Report("Write Ded Files");
    _chunking.Report("Chunking");
    std::cout << std::endl;
    std::cout << "Timings for parallel execution blocks (summed over all threads)" << std::endl;
    for (auto it = _sliceTimers.begin(); it != _sliceTimers.end(); ++it)
    {
        it->second->Report(it->first);
    }

    std::cout << std::endl;
    setup.Report("Setup data structures");
    movie.Report("Movie");
}

using WorkList = std::vector<std::tuple<unsigned, std::shared_ptr<Slicer>>>;

bool Compare(const std::pair<unsigned, unsigned>& i1, const std::pair<unsigned, unsigned>& i2)
{
    return i1.second > i2.second;
}

void MovieMaker::CreateMovie(
                     const int coord, 
                     const SliceOptions* sliceOptions, 
                     const std::vector<VariablePtr>& vars,
                     const std::string& fileName,
                     vtkMultiBlockDataSet* _domain)
{
    std::cout << std::endl;
    if (coord < 0 || coord > 2)
    {
        std::cout << "What the smeg are you doing - we live in 3 dimensions" << std::endl;
        return;
    }

    int numFrames = int((sliceOptions->End - sliceOptions->Start) / sliceOptions->Split) + 1;
    float start = sliceOptions->Start - float(1e-4);
    float sliceDist = sliceOptions->Split;

    // build chunks
    // we are going to slice up the domain of interest into chunks
    // each chunk can be processed in parallel, there will be multiple slices per chunk
    _chunking.Start();
    unsigned nChunks = omp_get_max_threads();
    unsigned divisor = numFrames/3;
	
    if(nChunks <= 50)
    {
        nChunks = omp_get_max_threads() * 5; //ideal sort of number
        nChunks = std::min(nChunks, (unsigned)(numFrames / 3));
    }
    else
    {
        nChunks = omp_get_max_threads() * 1; 
        nChunks = std::min(nChunks, (unsigned)(numFrames));
    }
    std::vector<vtkSmartPointer<vtkMultiBlockDataSet>> chunks(nChunks);
    auto dChunk = ((float)numFrames * sliceDist) / (float)nChunks;

    std::cout << "Chunking domain for processing, number " << nChunks << " size " << dChunk << std::endl;

    std::vector<double> mn(3);
    std::vector<double> mx(3);
    _boxOfInterest->GetXMin(&mn[0]);
    _boxOfInterest->GetXMax(&mx[0]);
    
    
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < nChunks; ++i)
    {            
        auto ifl = float(i);     
        auto box = vtkSmartPointer<vtkBox>::New();
        if (coord == 0)
        {
            box->SetXMin(start + ifl * dChunk, mn[1], mn[2]);
            box->SetXMax(start + (ifl + 1.0) * dChunk, mx[1], mx[2]);
        }
        else if (coord == 1)
        {
            box->SetXMin(mn[0], start + ifl * dChunk, mn[2]);
            box->SetXMax(mx[0], start + (ifl + 1.0) * dChunk, mx[2]);
        }
        else
        {
            box->SetXMin(mn[0], mn[1], start + ifl * dChunk);
            box->SetXMax(mx[0], mx[1], start + (ifl + 1.0) * dChunk);
        }
        // Create a local shallow copy of _domain to avoid threading issues
        auto domainCopy = vtkSmartPointer<vtkMultiBlockDataSet>::New();
        domainCopy->ShallowCopy(_domain);

        auto clip = vtkSmartPointer<vtkExtractGeometry>::New();
        clip->SetImplicitFunction(box);
        clip->SetInputData(domainCopy);
        clip->ExtractBoundaryCellsOn();

        // Perform the clipping operation
        clip->Update();
        auto mbc = vtkSmartPointer<vtkMultiBlockDataSet>::New();
        mbc->ShallowCopy(clip->GetOutputDataObject(0));       
        chunks[i] = mbc;
    }
    _chunking.Stop();

    // build worklist
    std::vector<std::vector<std::tuple<unsigned, float>>> workList(nChunks);

    unsigned currentChunk = 0;
    for (int s = 0; s < numFrames; ++s)
    {
        float x = start + float(s) * sliceDist;

        if(x < start + float(currentChunk+1)*dChunk)
        {
            workList[currentChunk].push_back({s, x});
        }
        else 
        {
            currentChunk++;
            workList[currentChunk].push_back({s, x});
        }
    }

    // build slicers

    std::vector<vtkSmartPointer<vtkPlane>> planes(nChunks);
    std::vector<vtkSmartPointer<vtkPlaneCutter>> cutters(nChunks);
    std::vector<std::pair<unsigned, unsigned>> complexity(nChunks);

    for (int i = 0; i < nChunks; ++i)
    {
        auto plane = vtkSmartPointer<vtkPlane>::New();
        plane->SetOrigin(0, 0, 0);
        if (coord == 0)
        {
            plane->SetNormal(1, 0, 0);
        }
        else if (coord == 1)
        {
            plane->SetNormal(0, 1, 0);
        }
        else
        {
            plane->SetNormal(0, 0, 1);
        }

        auto cutter = vtkSmartPointer<vtkPlaneCutter>::New();
        cutter->SetInputData(chunks[i]);
        cutter->SetPlane(plane);

        planes[i] = plane;
        cutters[i] = cutter;
        complexity[i] = std::make_pair(i, chunks[i]->GetNumberOfCells() * workList[i].size());
    }

    std::vector<std::shared_ptr<Slicer>> slicers(numFrames);

    std::vector<WorkList> workListSlicers(nChunks);
    
    for (unsigned i = 0; i < workList.size(); ++i)
    {
        for (auto it2 : workList[i])
        {
            const auto [sliceId, pos] = it2;
            auto slicer = std::make_shared<Slicer>(vars, coord, pos, _brakeDuct);
            workListSlicers[i].push_back({sliceId, slicer});
            slicers[sliceId] = slicer;
        }
    }

    // sort the work so we process the big chunks first
    std::sort(complexity.begin(), complexity.end(), Compare);

    // execute
    unsigned running = 1;
    auto cstr = coord == 1 ? "Y" : "Z";
    cstr = coord == 0 ? "X" : cstr;
    
    unsigned progress = 0;    
    	
    #pragma omp parallel for schedule(dynamic)
    for (unsigned cn1 = 0; cn1 < nChunks; ++cn1)
    {
        auto cn = complexity[cn1].first;

        auto plane = vtkSmartPointer<vtkPlane>::New();
        plane = planes[cn];

        auto cutter = vtkSmartPointer<vtkPlaneCutter>::New();
        cutter = cutters[cn];

        auto wList = workListSlicers[cn];
        for (int i = 0; i < wList.size(); ++i)
        {
            const auto& [sliceId, slicer] = wList[i];
            slicer->Slice(plane, cutter, _sliceTimers);
            // Use atomic increment to track progress
            #pragma omp atomic
            progress++;
            //Print progress from the main thread periodically (to avoid flooding the output)
            if (progress % 10 == 0 || progress == numFrames) 
            {	
                std::cout << "\rSlicing " << cstr << " slice: " << progress << "/" << numFrames << " ("	
                    << int(100 * (progress) / numFrames) << "%)" << std::endl;	
            }
        }	
    }

    // write
    _dedfilewrite.Start();

    std::cout << std::endl << "Writing ded file" << std::endl;

    auto dedFile = std::make_shared<DedFileWriter>(
        _outputDir + "/" + _filePrefix + sliceOptions->Prefix + fileName);
    dedFile->WriteHeader(_caseName, coord, sliceOptions->Start, sliceDist, numFrames, _conf, _boxOfInterest);

    for (unsigned i = 0; i < numFrames; ++i)
    {
        dedFile->WriteSlice(slicers[i]);
    }
    
    slicers.clear();

    _dedfilewrite.Stop();
}
