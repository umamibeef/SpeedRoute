//
//  RouterGraphics.cpp
//  SpeedRoute
//
//  Created by Michel Kakulphimp on 2018-03-18.
//  Copyright © 2018 Michel Kakulphimp. All rights reserved.
//

// Program Includes
#include "ResourcePath.hpp"
#include "ProgramOptions.hpp"
#include "FileParser.hpp"
#include "ConnectionGraph.hpp"
#include "Graphics.hpp"

extern "C"
{
#include "GraphWalk.h"
#include "OpenClApp.h"
}

static const int ARCH_PADDING = 2;
static int g_sideLength;

void outputGrid(int * weightArray)
{
    std::ofstream myOutputFile("grid.csv", std::ios::out);
    
    for(int row = 0; row < g_sideLength; row++)
    {
        for(int col = 0; col < g_sideLength; col++)
        {
            if(weightArray[col + row * g_sideLength] == 0)
            {
                myOutputFile << ",";
            }
            else
            {
                myOutputFile << std::to_string(weightArray[col + row * g_sideLength]) << ",";
            }
        }
        myOutputFile << std::endl;
    }
    
    myOutputFile.close();
}

int main(int argc, char *argv[])
{
    // Validate and grab program options
    ProgramOptions programOptions(argc, argv);
    programOptions.validate();
    programOptions_t options = programOptions.getOptions();
    
    // Parse the net and placement files
    FileParser fileParse(options.netFilenameIn, options.placementFilenameIn);
    if(!fileParse.parseInput())
    {
        return EXIT_FAILURE;
    }
    parsedInputStruct_t input = fileParse.getParsedInput();
    
    // Instantiate a new connection graph using the placement's col, row, and some padding
    ConnectionGraph connectionGraph(input.placement, input.numCols, input.numRows, ARCH_PADDING);
    // Grab the C-friendly graph data
    graphData_t graphData = connectionGraph.getGraphData();
    netData_t netData = connectionGraph.getNetVectors(input.nets, input.placement);

    // If OpenCL is being used, get a queue pointer and setup the memory structures
    if(options.openClEnableFlag)
    {
        OpenCl_Init(options.openClDeviceId);
        OpenCl_GraphWalk_InitGraphArrays(
                                         graphData.vertexArrayPointer,
                                         graphData.edgeArrayPointer,
                                         graphData.vertexArraySize,
                                         graphData.edgeArraySize
                                         );
        OpenCl_GraphWalk_InitWavefrontData(graphData.vertexArraySize);
    }
    
    // Update arch side length
    g_sideLength = graphData.sideLength;
    
    // Initialize the graph walker arrays
    GraphWalk_InitWalkData(graphData, netData, 8);
    // Initialize the net status array
    GraphWalk_InitNetStatus();
    
    // Route nets
    // Initiailize the weight array
    GraphWalk_InitWeight();
    // Initialize the routing arrays
    GraphWalk_InitNetRoutes();
    std::cout << "Routing starting..." << std::endl << std::endl;
    // Record the start time
    clock_t startTIme = clock();
    bool routeFailed = false;
    for(int i = 0; i < input.nets.size(); i++)
    {
        // Init a new net route
        GraphWalk_NewNetRoute();
        // Route the nets
        if(!GraphWalk_RouteNet(options.openClEnableFlag, i))
        {
            routeFailed = true;
            break;
        }
    }
    // Record the end time
    clock_t endTime = clock();
    
    if(!routeFailed)
    {
        GraphWalk_DebugPrintGrid(PRIO_NORM, const_cast<char *>("Final weights"), GraphWalk_GetWeightArray());
        outputGrid(GraphWalk_GetWeightArray());
        double timeInSeconds = (endTime - startTIme) / (double) CLOCKS_PER_SEC;
        std::cout << "Routing finished in: " << timeInSeconds << " s" << std::endl;
    }
    
    // Run the graphics for the router
//    Graphics graphics;
//    graphics.run();
    
    return EXIT_SUCCESS;
}
