#include "global.h"

/**
 * @brief 
 * used to check if a condition provided in PATH is valid 
 * N = true
 * E = false
 */
pair<pair<string, bool>, bool> parseCondition(string cond)
{
    pair<pair<string, bool>, bool> ret;
    ret.second = true;
    auto open_bracket = cond.find("(");
    auto close_bracket = cond.find(")");
    if (open_bracket != string::npos && close_bracket != string::npos) {
        ret.first.first = cond.substr(0, open_bracket);
        string NE = cond.substr(open_bracket + 1, close_bracket - open_bracket - 1);
        if (NE == "N")
            ret.first.second = true;
        else if (NE == "E")
            ret.first.second = false; 
        else
            ret.second = false;
    }
    else
        ret.second = false;
    return ret;

}

/**
 * @brief 
 * SYNTAX: RES <- PATH <graph_name> <src_NodeID> <dest_NodeID> WHERE <conditions>
 * for where, 0 -> bool 0, 1 -> bool 1, 2 -> missing
 */
bool syntacticParsePATH()
{
    logger.log("syntacticParsePATH");
    int num_toks = tokenizedQuery.size();
    if (num_toks < 6)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }
    parsedQuery.queryType = PATH;
    parsedQuery.loadGraphRelationName = tokenizedQuery[3];
    parsedQuery.src_node = tokenizedQuery[4];
    parsedQuery.dest_node = tokenizedQuery[5];
    if (num_toks > 6)
    {
        if (tokenizedQuery[6] != "WHERE")
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
        for(int i = 7; i < num_toks; i++)
        {
            pair<pair<string, bool>, bool> parsedCond = parseCondition(tokenizedQuery[i]);
            char boolCond = 2;
            if (!parsedCond.second)
            {
                cout << "SYNTAX ERROR" << endl;
                return false;
            }
            if(i + 1 < num_toks && tokenizedQuery[i+1] == "==")
            {
                if (i + 2 < num_toks && (tokenizedQuery[i+2] == "1" || tokenizedQuery[i+2] == "0"))
                {
                    if (tokenizedQuery[i+2] == "1")
                        boolCond = 1;
                    else
                        boolCond = 0;
                }
                else
                {
                    cout << "SYNTAX ERROR" << endl;
                    return false;
                }
                i +=2;
            }
            if(i + 1 < num_toks)
            {
                if (tokenizedQuery[i+1] == "AND")
                    i++;
                else
                {
                    cout << "SYNTAX ERROR" << endl;
                    return false;
                }
            }
            parsedQuery.path_condtions.push_back({parsedCond.first, boolCond});
        }
    }
    return true;
}


bool semanticParsePATH()
{
    logger.log("semanticParsePATH");
        string GraphName = parsedQuery.loadGraphRelationName;

    // check for both types of graphs
    string NodesRelationName = GraphName + "_Nodes_";
    string EdgesRelationName = GraphName + "_Edges_";
    string SN_name = GraphName + "_SN_";
    string SE_name = GraphName + "_SE_";

    // Error if: (i) U Node or U edge don't exist && (ii) D Node or D edge don't exist 
    if (tableCatalogue.isTable(NodesRelationName + "U") && tableCatalogue.isTable(EdgesRelationName + "U")
        && tableCatalogue.isTable(SN_name + "U") && tableCatalogue.isTable(SE_name + "U"))
    {
        parsedQuery.graphType = UNDIRECTED;
        return true;
    }
    else if (tableCatalogue.isTable(NodesRelationName + "D") && tableCatalogue.isTable(EdgesRelationName + "D")
        && tableCatalogue.isTable(SN_name + "D") && tableCatalogue.isTable(SE_name + "D"))
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

/**
 * @brief
* struct for heap
*/
struct heapNode {
    int node;
    long long weight;
    vector<bool> nodeinfo;
    vector<bool> edgeinfo;
};

struct compareheapNode {
    bool operator()(heapNode const& t1, heapNode const& t2) {
        return t1.weight > t2.weight;
    }
};

/**
 * @brief Performs Binary Search on the SortedNodesTable to find the NodeID
 * and returns its attributes. 
 */
vector<bool> getNodeInfo(int src)
{
    string GraphName = parsedQuery.loadGraphRelationName;
    string type = (parsedQuery.graphType == DIRECTED) ? "D" : "U";
    
    // Access the augmented table created in load()
    string tableName = GraphName + "_SN_Augmented_" + type;
    
    Table *table = tableCatalogue.getTable(tableName);
    if (!table) return {};

    int low = 0;
    int high = table->blockCount - 1;
    int targetPage = -1;

    // 1. Binary Search over Pages
    while (low <= high) {
        int mid = low + (high - low) / 2;
        
        // Use table metadata instead of page.rowCount to avoid private access error
        int rowCount = table->rowsPerBlockCount[mid];
        
        if (rowCount == 0) {
             high = mid - 1; 
             continue; 
        }

        Page page = bufferManager.getPage(tableName, mid);
        int firstId = page.getRow(0)[0];
        int lastId = page.getRow(rowCount - 1)[0];

        if (src < firstId) {
            high = mid - 1;
        } else if (src > lastId) {
            low = mid + 1;
        } else {
            targetPage = mid;
            break;
        }
    }

    if (targetPage == -1) return {}; // Node not found

    // 2. Linear Search within the identified Page
    Page page = bufferManager.getPage(tableName, targetPage);
    int rowCount = table->rowsPerBlockCount[targetPage];
    
    for (int i = 0; i < rowCount; i++) {
        vector<int> row = page.getRow(i);
        if (row[0] == src) {
            // Row Format: [ID, Attr1, ..., AttrN, EdgePage, EdgeRow]
            vector<bool> info;
            // Extract attributes (skip ID at index 0, skip last 2 indices)
            for (int k = 1; k < row.size() - 2; k++) {
                info.push_back(row[k] != 0);
            }
            return info;
        }
    }

    return {};
}

/**
 * @brief Retrieves edge attributes between src and dest.
 * Uses Binary Search on SortedNodesTable to find the start of the edge list,
 * then linearly scans SortedEdgesTable to find the specific destination.
 */
vector<bool> getEdgeInfo(int src, int dest)
{
    string GraphName = parsedQuery.loadGraphRelationName;
    string type = (parsedQuery.graphType == DIRECTED) ? "D" : "U";
    
    // 1. Identify Tables
    string nodeTableName = GraphName + "_SN_Augmented_" + type;
    string edgeTableName = GraphName + "_SE_" + type;

    Table* nodeTable = tableCatalogue.getTable(nodeTableName);
    Table* edgeTable = tableCatalogue.getTable(edgeTableName);

    if (!nodeTable || !edgeTable) return {};

    // ---------------------------------------------------------
    // STEP 1: Find 'src' in SortedNodesTable (Binary Search)
    // ---------------------------------------------------------
    int low = 0;
    int high = nodeTable->blockCount - 1;
    int nodePageIdx = -1;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        int rowCount = nodeTable->rowsPerBlockCount[mid];
        
        if (rowCount == 0) {
            high = mid - 1;
            continue;
        }

        Page page = bufferManager.getPage(nodeTableName, mid);
        int firstId = page.getRow(0)[0];
        int lastId = page.getRow(rowCount - 1)[0];

        if (src < firstId) {
            high = mid - 1;
        } else if (src > lastId) {
            low = mid + 1;
        } else {
            nodePageIdx = mid;
            break;
        }
    }

    if (nodePageIdx == -1) return {}; // Source node not found in graph

    // ---------------------------------------------------------
    // STEP 2: Extract Start Indices from Node Table
    // ---------------------------------------------------------
    Page nodePage = bufferManager.getPage(nodeTableName, nodePageIdx);
    int nodeRows = nodeTable->rowsPerBlockCount[nodePageIdx];
    
    int startEdgePage = -1;
    int startEdgeRow = -1;
    bool foundSrc = false;

    for (int i = 0; i < nodeRows; i++) {
        vector<int> row = nodePage.getRow(i);
        if (row[0] == src) {
            // Last two columns are [edgePage, edgeRowIndex]
            int cols = row.size();
            startEdgePage = row[cols - 2];
            startEdgeRow = row[cols - 1];
            foundSrc = true;
            break;
        }
    }

    // If node exists but has no edges (indices are -1)
    if (!foundSrc || startEdgePage == -1 || startEdgeRow == -1) return {}; 

    // ---------------------------------------------------------
    // STEP 3: Find 'dest' in SortedEdgesTable (Linear Scan)
    // ---------------------------------------------------------
    // We start exactly where the node told us to.
    int currPageIdx = startEdgePage;
    int currRowIdx = startEdgeRow;

    while (currPageIdx < edgeTable->blockCount) {
        Page edgePage = bufferManager.getPage(edgeTableName, currPageIdx);
        int edgeRows = edgeTable->rowsPerBlockCount[currPageIdx];

        // Iterate through rows in the current page
        for (int r = currRowIdx; r < edgeRows; r++) {
            vector<int> row = edgePage.getRow(r);
            
            // Row Format: [Source, Dest, Attr1, Attr2...]
            int s = row[0];
            int d = row[1];

            if (s != src) {
                // We moved past the block of edges for 'src'. 
                // Since edges are sorted by source, we can stop early.
                return {}; 
            }

            if (d == dest) {
                // Found the edge! Return attributes (skip first 2 cols)
                vector<bool> info;
                for (size_t k = 2; k < row.size(); k++) {
                    info.push_back(row[k] != 0);
                }
                return info;
            }
        }

        // Move to the next page, reset row index to 0
        currPageIdx++;
        currRowIdx = 0;
    }

    return {}; // Edge (src -> dest) not found
}


vector<bool> hadamardproduct(vector<bool> &v1, vector<bool> &v2, bool negate)
{
    vector<bool> prod(v1.size(), 0);
    for (int i = 0; i < v1.size(); i++)
    {
        if (negate)
            prod[i] = (!v1[i] & !v2[i]); 
        else
            prod[i] = (v1[i] & v2[i]);
    }
    return prod;
}

bool anyTruePrefix(const vector<bool>& v, int len) {
    int L = len;
    if (L < 0)
        L = 0;
    if (L > (int)v.size())
        L = (int)v.size();
    for (int i = 0; i < L; i++)
        if (v[i]) return true;
    return false;
}


void executePATH()
{
    /*
    I need to basically run djiskstra while making sure i satisfy all the conditons 
    1) need to check if the src and dest nodes exist
    2) check if the conditions given are valid
    
    maintain a heap, add all the edges of the src node into it (if it satisfies the conditions)
    to access the edges, use the SN table to obtain the offset of the edge table, index that page.
    add the edge to the heap if
    (i) dest satisfies node conditions
    (ii) it satisfies the edge conditions
    pop keep going until heap is empty 
    maintain visited and heap in memory 
    */
   logger.log("executePATH");
   int n;
   int m;
   int src = stoi(parsedQuery.src_node);
   int dest = stoi(parsedQuery.dest_node);
   //((attribute, N/E), 1/0/missing)
   vector<pair<pair<string,bool>, char>> conditions = parsedQuery.path_condtions;

    vector<bool> srcNInfo = getNodeInfo(src);
   // --- Build correct initial state for ANY(...) ---
    // nodeinfo = [cand1 | cand0], size = 2*A
    // cand1[i]: attribute i can still witness ANY(N)==1
    // cand0[i]: attribute i can still witness ANY(N)==0
    int A0 = (int)srcNInfo.size();
    vector<bool> initNodeInfo(2 * A0, 0);
    for (int i = 0; i < A0; i++)
    {
        initNodeInfo[i] = srcNInfo[i];        // cand1 starts with src values
        initNodeInfo[A0 + i] = !srcNInfo[i];  // cand0 starts as complement of src
    }

    // slots for missing-== fixed EDGE attributes (Bj(E))
    vector<int> missingEdgeSlot(conditions.size(), -1);
    int missingEdgeCnt = 0;
    for (int ci = 0; ci < (int)conditions.size(); ci++)
    {
        if (conditions[ci].second == 2 &&
            conditions[ci].first.second == false)
        {
            missingEdgeSlot[ci] = missingEdgeCnt++;
        }
    }

    priority_queue<heapNode, vector<heapNode>, compareheapNode> pq;

    // edgeinfo prefix = [cand1 | cand0], size = 2*m
    // tail = (isSet,value) for each missing-edge fixed condition
    vector<bool> totEdgeInfo(2 * m, 1);
    totEdgeInfo.resize(2 * m + 2 * missingEdgeCnt, 0);

    pq.push(heapNode({src, 0, initNodeInfo, totEdgeInfo}));
   vector<long long> d(n, LONG_MAX);
   d[src] = 0;


   while(!pq.empty())
   {
    heapNode top = pq.top();
    int v = top.node;
    long long d_v = top.weight;
    vector<bool> nodeInfo = top.nodeinfo;
    vector<bool> edgeInfo = top.edgeinfo;
    pq.pop();

    int baseNodeAttr = (int)nodeInfo.size();                     // = 2*A
    int baseEdgeAttr = (int)edgeInfo.size() - 2 * missingEdgeCnt; // = 2*m

    int A = baseNodeAttr / 2;  // node attribute count
    int B = baseEdgeAttr / 2;  // edge attribute count


    //get outgoing edges from popped node. 
    for (int i = 1; i < 1; i++)
    {
        int u;
        int w;
        vector<bool> uNInfo = getNodeInfo(u);
        vector<bool> uEInfo = getEdgeInfo(v, u);
        //check conditions
        vector<bool> nextNodeState = nodeInfo;  
        vector<bool> nextEdgeState = edgeInfo;
        bool ok = true;
        for(int ci = 0; ci < (int)conditions.size(); ci++)
        {
            auto condition = conditions[ci];

            if(condition.second == 2)
            {
                int atrinum = stoi(condition.first.first.substr(1,string::npos)) - 1;
                // true == N
                if (condition.first.second) 
                {
                    if (uNInfo[atrinum] != srcNInfo[atrinum])
                    {
                        ok = false;
                        break;
                    }
                }
                // false == E
                else 
                {
                    int slot = missingEdgeSlot[ci];
                    if (slot < 0)
                    {
                        ok = false;
                        break;
                    }

                    // isSet
                    int flagPos = baseEdgeAttr + 2 * slot;
                    //value       
                    int valPos  = baseEdgeAttr + 2 * slot + 1;  

                    if (!nextEdgeState[flagPos])
                    {
                        // First edge on this candidate path: set req uniform val
                        nextEdgeState[flagPos] = true;
                        nextEdgeState[valPos] = uEInfo[atrinum];
                    }
                    else
                    {
                        // subsequent edges match the chosen val
                        if (nextEdgeState[valPos] != uEInfo[atrinum])
                        {
                            ok = false;
                            break;
                        }
                    }
                }
            }
            else if (condition.first.first == "ANY")
            {
                // true == N
                if (condition.first.second)
                {
                    // Update candidate masks:
                    // cand1[i] &= val(i), cand0[i] &= !val(i)
                    for (int k = 0; k < A; k++)
                    {
                        bool val = false;
                        if (k < (int)uNInfo.size())
                            val = uNInfo[k];

                        nextNodeState[k] = nextNodeState[k] & val;           // cand1
                        nextNodeState[A + k] = nextNodeState[A + k] & (!val); // cand0
                    }

                    bool found = false;
                    if (condition.second == 1)
                    {
                        // any candidate attribute remains for == 1
                        found = anyTruePrefix(nextNodeState, A);
                    }
                    else
                    {
                        // any candidate attribute remains for == 0
                        for (int k = 0; k < A; k++)
                        {
                            if (nextNodeState[A + k])
                            {
                                found = true;
                                break;
                            }
                        }
                    }

                    if (!found)
                    {
                        ok = false;
                        break;
                    }
                }
                // false == E
                else
                {
                    for (int k = 0; k < B; k++)
                    {
                        bool val = false;
                        if (k < (int)uEInfo.size())
                            val = uEInfo[k];

                        nextEdgeState[k] = nextEdgeState[k] & val;            // cand1
                        nextEdgeState[B + k] = nextEdgeState[B + k] & (!val); // cand0
                    }

                    bool found = false;
                    if (condition.second == 1)
                    {
                        found = anyTruePrefix(nextEdgeState, B); // cand1 part
                    }
                    else
                    {
                        for (int k = 0; k < B; k++)
                        {
                            if (nextEdgeState[B + k])
                            {
                                found = true;
                                break;
                            }
                        }
                    }

                    if (!found)
                    {
                        ok = false;
                        break;
                    }
                }
            }
            else
            {
                int atrinum = stoi(condition.first.first.substr(1,string::npos)) - 1;
                if(condition.first.second)
                {
                    if(uNInfo[atrinum] != condition.second)
                    {
                        ok = false;
                        break; 
                    }
                }
                // false == E
                else
                {
                    if(uEInfo[atrinum] != condition.second)
                    {
                        ok = false;
                        break; 
                    }
                }
            }
        }
        if (ok && (d[v] + w < d[u]))
        {
            d[u] = d[v] + w;
            pq.push(heapNode({u, d[u], nextNodeState, nextEdgeState}));
        }
    }
   }

   //push
    return;
}