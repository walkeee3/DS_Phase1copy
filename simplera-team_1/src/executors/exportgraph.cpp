#include "global.h"

/**
 * @brief 
 * SYNTAX: EXPORT GRAPH <name>
 */

bool syntacticParseEXPORTGRAPH()
{
    logger.log("syntacticParseEXPORTGRAPH");
    if (tokenizedQuery.size() != 3)
    {
        cout << "SYNTAX ERROR" << endl;
        cout << "WHAT" << endl;
        cout << tokenizedQuery.size() << endl;
        return false;
    }
    parsedQuery.queryType = EXPORTGRAPH;
    parsedQuery.exportRelationName = tokenizedQuery[2];
    return true;
}

bool semanticParseEXPORTGRAPH()
{
    logger.log("semanticParseEXPORTGRAPH");
    string GraphName = parsedQuery.exportRelationName;

    // check for both types of graphs
    string NodesRelationName = GraphName + "_Nodes_";
    string EdgesRelationName = GraphName + "_Edges_";

    // Error if: (i) U Node or U edge don't exist && (ii) D Node or D edge don't exist 
    if (tableCatalogue.isTable(NodesRelationName + "U") && tableCatalogue.isTable(EdgesRelationName + "U"))
    {
        parsedQuery.graphType = UNDIRECTED;
        return true;
    }
    else if (tableCatalogue.isTable(NodesRelationName + "D") && tableCatalogue.isTable(EdgesRelationName + "D"))
    {
        parsedQuery.graphType = DIRECTED;
        return true;
    }
    else
    {
        cout << "SEMANTIC ERROR: Graph doesn't exist" << endl;
        return false;
    }
}

void executeEXPORTGRAPH()
{
    logger.log("executeEXPORTGRAPH");
    string GraphName = parsedQuery.exportRelationName;
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

    Table* edges = tableCatalogue.getTable(EdgesRelationName);
    Table* nodes = tableCatalogue.getTable(NodesRelationName);
    edges->makePermanent();
    nodes->makePermanent();
    return;
}