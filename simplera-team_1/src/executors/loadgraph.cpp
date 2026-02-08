#include "global.h"

/**
 * @brief 
 * SYNTAX: LOAD GRAPH relation_name U/D
 */
bool syntacticParseLOADGRAPH()
{
    logger.log("syntacticParseLOADGRAPH");
    if (tokenizedQuery.size() != 4)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }
    parsedQuery.queryType = LOADGRAPH;
    parsedQuery.loadGraphRelationName = tokenizedQuery[2];
    if(tokenizedQuery[3] == "D")
    {
        parsedQuery.graphType = DIRECTED;
    }
    else
    {
        parsedQuery.graphType = UNDIRECTED;
    }
    return true;
}

bool semanticParseLOADGRAPH()
{
    logger.log("semanticParseLOADGRAPH");

    string GraphName = parsedQuery.loadGraphRelationName;
    GraphType graphType = parsedQuery.graphType;

    string type;
    if(graphType == DIRECTED)
    {
        type = "D";
    }
    else
    {
        type = "U";
    }

    string NodesRelationName = GraphName + "_Nodes_" + type;
    string EdgesRelationName = GraphName + "_Edges_" + type;
    string sortedNodes = GraphName + "_SN_" + type;
    string sortedEdges = GraphName + "_SE_" + type;

    if (tableCatalogue.isTable(NodesRelationName) && tableCatalogue.isTable(EdgesRelationName) &&
        tableCatalogue.isTable(sortedNodes) && tableCatalogue.isTable(sortedEdges))
    {
        cout << "SEMANTIC ERROR: Relation for Nodes or/and Edges already exists" << endl;
        return false;
    }

    if (!isGraphExists(GraphName, type))
    {
        cout << "SEMANTIC ERROR: Data file doesn't exist" << endl;
        return false;
    }
    return true;
}

void executeLOADGRAPH()
{
    logger.log("executeLOADGRAPH");

    string GraphName = parsedQuery.loadGraphRelationName;
    GraphType graphType = parsedQuery.graphType;

    string type;
    if(graphType == DIRECTED)
    {
        type = "D";
    }
    else
    {
        type = "U";
    }

    Graph *MyGraph = new Graph(GraphName, type);

    MyGraph->load();

    delete MyGraph;

    return;
}