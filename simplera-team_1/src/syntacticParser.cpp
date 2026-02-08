#include "global.h"

bool syntacticParse()
{
    logger.log("syntacticParse");
    string possibleQueryType = tokenizedQuery[0];

    if (tokenizedQuery.size() < 2)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    if (possibleQueryType == "CLEAR")
        return syntacticParseCLEAR();
    else if (possibleQueryType == "INDEX")
        return syntacticParseINDEX();
    else if (possibleQueryType == "LIST")
        return syntacticParseLIST();
    else if (possibleQueryType == "LOAD")
    {
        if(tokenizedQuery[1] == "GRAPH")
        {
            return syntacticParseLOADGRAPH();
        }
        else
        {
            return syntacticParseLOAD();
        }        
    }
    else if (possibleQueryType == "PRINT")
        return syntacticParsePRINT();
    else if (possibleQueryType == "RENAME")
        return syntacticParseRENAME();
    else if(possibleQueryType == "EXPORT")
    {
        if(tokenizedQuery[1] == "GRAPH")
        {
            return syntacticParseEXPORTGRAPH();
        }
        else
        {
            return syntacticParseEXPORT();
        }       
    }
    else if(possibleQueryType == "SOURCE")
        return syntacticParseSOURCE();
    else
    {
        string resultantRelationName = possibleQueryType;
        if (tokenizedQuery[1] != "<-" || tokenizedQuery.size() < 3)
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
        possibleQueryType = tokenizedQuery[2];
        if (possibleQueryType == "PROJECT")
            return syntacticParsePROJECTION();
        else if (possibleQueryType == "SELECT")
            return syntacticParseSELECTION();
        else if(possibleQueryType == "PATH")
            return syntacticParsePATH();
        else if (possibleQueryType == "JOIN")
            return syntacticParseJOIN();
        else if (possibleQueryType == "CROSS")
            return syntacticParseCROSS();
        else if (possibleQueryType == "DISTINCT")
            return syntacticParseDISTINCT();
        else if (possibleQueryType == "SORT")
            return syntacticParseSORT();
        else
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
    }
    return false;
}

ParsedQuery::ParsedQuery()
{
}

void ParsedQuery::clear()
{
    logger.log("ParseQuery::clear");
    this->queryType = UNDETERMINED;

    this->graphType = UNKNOWN;
    this->loadGraphRelationName = "";
    this->src_node = "";
    this->dest_node = "";
    this->path_condtions.clear();

    this->clearRelationName = "";

    this->crossResultRelationName = "";
    this->crossFirstRelationName = "";
    this->crossSecondRelationName = "";

    this->distinctResultRelationName = "";
    this->distinctRelationName = "";

    this->exportRelationName = "";

    this->indexingStrategy = NOTHING;
    this->indexColumnName = "";
    this->indexRelationName = "";

    this->joinBinaryOperator = NO_BINOP_CLAUSE;
    this->joinResultRelationName = "";
    this->joinFirstRelationName = "";
    this->joinSecondRelationName = "";
    this->joinFirstColumnName = "";
    this->joinSecondColumnName = "";

    this->loadRelationName = "";

    this->printRelationName = "";

    this->projectionResultRelationName = "";
    this->projectionColumnList.clear();
    this->projectionRelationName = "";

    this->renameFromColumnName = "";
    this->renameToColumnName = "";
    this->renameRelationName = "";

    this->selectType = NO_SELECT_CLAUSE;
    this->selectionBinaryOperator = NO_BINOP_CLAUSE;
    this->selectionResultRelationName = "";
    this->selectionRelationName = "";
    this->selectionFirstColumnName = "";
    this->selectionSecondColumnName = "";
    this->selectionIntLiteral = 0;

    this->sortingStrategy = NO_SORT_CLAUSE;
    this->sortResultRelationName = "";
    this->sortColumnName = "";
    this->sortRelationName = "";

    this->sourceFileName = "";
}

/**
 * @brief Checks to see if source file exists. Called when LOAD command is
 * invoked.
 *
 * @param tableName 
 * @return true 
 * @return false 
 */
bool isFileExists(string tableName)
{
    string fileName = "../data/" + tableName + ".csv";
    struct stat buffer;
    return (stat(fileName.c_str(), &buffer) == 0);
}

/**
 * @brief Checks to see if source file exists. Called when LOAD GRAPH command is
 * invoked.
 *
 * @param GraphName 
 * @param GraphType
 * @return true 
 * @return false 
 */
bool isGraphExists(string GraphName, string type)
{
    string NodesfileName = "../data/" + GraphName + "_Nodes_" + type + ".csv";
    string EdgesfileName = "../data/" + GraphName + "_Edges_" + type + ".csv";
    struct stat buffer1;
    struct stat buffer2;
    return (stat(NodesfileName.c_str(), &buffer1) == 0) && (stat(EdgesfileName.c_str(), &buffer2) == 0);
}

/**
 * @brief Checks to see if source file exists. Called when SOURCE command is
 * invoked.
 *
 * @param tableName 
 * @return true 
 * @return false 
 */
bool isQueryFile(string fileName){
    fileName = "../data/" + fileName + ".ra";
    struct stat buffer;
    return (stat(fileName.c_str(), &buffer) == 0);
}

/**
 * @brief Checks to see if source graph file exists. Called when SOURCE command is
 * invoked.
 *
 * @param GraphName 
 * @param GraphType 
 * @return true 
 * @return false 
 */
bool isQueryGraph(string GraphName, GraphType graphType)
{
    string type;
    if(graphType == DIRECTED)
    {
        type = "D";
    }
    else
    {
        type = "U";
    }
    string NodefileName = "../data/" + GraphName + "_Nodes_" + type + ".ra";
    string EdgesfileName = "../data/" + GraphName + "_Edges_" + type + ".ra";
    struct stat buffer1;
    struct stat buffer2;
    return (stat(NodefileName.c_str(), &buffer1) == 0) && (stat(EdgesfileName.c_str(), &buffer2) == 0);
}