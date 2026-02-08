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
 * @brief Retrieves outgoing edges from src.
 * Returns raw rows from SE table starting at src's edge pointer.
 * Each returned row is the SE row: [Source, Dest, ...]
 */
vector<vector<int>> getEdgeInfo(int src)
{
    string GraphName = parsedQuery.loadGraphRelationName;
    string type = (parsedQuery.graphType == DIRECTED) ? "D" : "U";

    string nodeTableName = GraphName + "_SN_Augmented_" + type;
    string edgeTableName = GraphName + "_SE_" + type;

    Table* nodeTable = tableCatalogue.getTable(nodeTableName);
    Table* edgeTable = tableCatalogue.getTable(edgeTableName);
    if (!nodeTable || !edgeTable) return {};

    // ---- Find src page in SN (binary search across blocks) ----
    int low = 0, high = nodeTable->blockCount - 1;
    int nodePageIdx = -1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int rowCount = nodeTable->rowsPerBlockCount[mid];
        if (rowCount == 0) { high = mid - 1; continue; }

        Page page = bufferManager.getPage(nodeTableName, mid);
        int firstId = page.getRow(0)[0];
        int lastId  = page.getRow(rowCount - 1)[0];

        if (src < firstId) high = mid - 1;
        else if (src > lastId) low = mid + 1;
        else { nodePageIdx = mid; break; }
    }
    if (nodePageIdx == -1) return {};

    // ---- Extract edge start pointer from SN row ----
    Page nodePage = bufferManager.getPage(nodeTableName, nodePageIdx);
    int nodeRows = nodeTable->rowsPerBlockCount[nodePageIdx];

    int startEdgePage = -1, startEdgeRow = -1;
    bool foundSrc = false;

    for (int i = 0; i < nodeRows; i++) {
        vector<int> row = nodePage.getRow(i);
        if (row[0] == src) {
            int cols = (int)row.size();
            startEdgePage = row[cols - 2];
            startEdgeRow  = row[cols - 1];
            foundSrc = true;
            break;
        }
    }
    if (!foundSrc || startEdgePage == -1 || startEdgeRow == -1) return {};

    // ---- Scan SE from that pointer while Source==src ----
    vector<vector<int>> edgeRows;
    int currPageIdx = startEdgePage;
    int currRowIdx = startEdgeRow;

    while (currPageIdx < edgeTable->blockCount) {
        Page edgePage = bufferManager.getPage(edgeTableName, currPageIdx);
        int edgeRowsCount = edgeTable->rowsPerBlockCount[currPageIdx];

        for (int r = currRowIdx; r < edgeRowsCount; r++) {
            vector<int> row = edgePage.getRow(r);
            if (row.empty()) continue;

            int s = row[0];
            if (s != src) {
                return edgeRows; // finished src's adjacency block
            }

            edgeRows.push_back(row); // keep raw row, do NOT bool-convert
        }

        // move to next page
        currPageIdx++;
        currRowIdx = 0;
    }

    return edgeRows;
}


// ---------- helpers to write tables ----------

static void dropIfExists(const string &tableName) {
    if (tableCatalogue.isTable(tableName)) {
        tableCatalogue.deleteTable(tableName);
    }
}

static void writeRowsToTable(Table *t, const string &name, const vector<vector<int>> &rows) {
    // rows can be empty: then just keep blockCount=0,rowCount=0 and only header exists (from constructor)
    t->rowCount = 0;
    t->blockCount = 0;
    t->rowsPerBlockCount.clear();

    if (rows.empty()) return;

    int maxR = (int)t->maxRowsPerBlock;
    int pageIdx = 0;
    vector<vector<int>> buf;
    buf.reserve(maxR);

    for (auto &r : rows) {
        buf.push_back(r);
        if ((int)buf.size() == maxR) {
            bufferManager.writePage(name, pageIdx++, buf, (int)buf.size());
            t->rowsPerBlockCount.push_back((int)buf.size());
            t->rowCount += (int)buf.size();
            t->blockCount++;
            buf.clear();
        }
    }
    if (!buf.empty()) {
        bufferManager.writePage(name, pageIdx++, buf, (int)buf.size());
        t->rowsPerBlockCount.push_back((int)buf.size());
        t->rowCount += (int)buf.size();
        t->blockCount++;
    }
}

static void materializePathGraph(
    const string &outGraph, const string &type,
    const string &srcGraph,
    const vector<int> &pathNodes,
    const vector<vector<int>> &pathEdges,
    int nodeAttrCount, int edgeAttrCount,
    unordered_map<int, vector<bool>> &nodeCache
) {
    // Names expected by your semantic parser + getNodeInfo/getEdgeInfo
    string outNodes = outGraph + "_Nodes_" + type;
    string outEdges = outGraph + "_Edges_" + type;
    string outSN    = outGraph + "_SN_"    + type;
    string outSE    = outGraph + "_SE_"    + type;
    string outSNA   = outGraph + "_SN_Augmented_" + type;

    // Remove old versions if they exist
    dropIfExists(outSNA);
    dropIfExists(outSE);
    dropIfExists(outSN);
    dropIfExists(outEdges);
    dropIfExists(outNodes);

    // Use same column headers as the source graph if available; else synthesize.
    vector<string> nodeCols, edgeCols;
    {
        Table *srcN = tableCatalogue.getTable(srcGraph + "_Nodes_" + type);
        Table *srcE = tableCatalogue.getTable(srcGraph + "_Edges_" + type);

        if (srcN) nodeCols = srcN->columns;
        if (srcE) edgeCols = srcE->columns;

        if (nodeCols.empty()) {
            nodeCols.push_back("NodeID");
            for (int i = 1; i <= nodeAttrCount; i++) nodeCols.push_back("A" + to_string(i));
        }
        if (edgeCols.empty()) {
            edgeCols.push_back("Src_NodeID");
            edgeCols.push_back("Dest_NodeID");
            edgeCols.push_back("Weight");
            for (int j = 1; j <= edgeAttrCount; j++) edgeCols.push_back("B" + to_string(j));
        }
    }

    // Build Nodes rows in path order: [NodeID, A1..An]
    vector<vector<int>> nodeRows;
    nodeRows.reserve(pathNodes.size());

    for (int nid : pathNodes) {
        if (nodeCache.find(nid) == nodeCache.end()) nodeCache[nid] = getNodeInfo(nid);
        const auto &attrs = nodeCache[nid];

        vector<int> r;
        r.reserve(1 + nodeAttrCount);
        r.push_back(nid);
        for (int i = 0; i < nodeAttrCount; i++) r.push_back(attrs[i] ? 1 : 0);
        nodeRows.push_back(std::move(r));
    }

    // Edges rows already in SE format: [Src, Dest, Weight, B1..Bm]
    vector<vector<int>> edgeRows = pathEdges;

    // Create base tables
    Table *tNodes = new Table(outNodes, nodeCols);
    tableCatalogue.insertTable(tNodes);
    writeRowsToTable(tNodes, outNodes, nodeRows);

    Table *tEdges = new Table(outEdges, edgeCols);
    tableCatalogue.insertTable(tEdges);
    writeRowsToTable(tEdges, outEdges, edgeRows);

    // Create sorted SN/SE (required by your semanticParsePATH existence checks + adjacency access)
    vector<vector<int>> snRows = nodeRows;
    vector<vector<int>> seRows = edgeRows;
    sort(snRows.begin(), snRows.end());
    sort(seRows.begin(), seRows.end());

    Table *tSN = new Table(outSN, nodeCols);
    tableCatalogue.insertTable(tSN);
    writeRowsToTable(tSN, outSN, snRows);

    Table *tSE = new Table(outSE, edgeCols);
    tableCatalogue.insertTable(tSE);
    writeRowsToTable(tSE, outSE, seRows);

    // Build first-edge pointers for augmented SN
    unordered_map<int, pair<int,int>> firstPtr; // node -> (page,row)
    firstPtr.reserve(snRows.size() * 2);

    int seMax = (int)tSE->maxRowsPerBlock;
    for (int i = 0; i < (int)seRows.size(); i++) {
        int s = seRows[i][0];
        if (firstPtr.find(s) == firstPtr.end()) {
            firstPtr[s] = { i / seMax, i % seMax };
        }
    }

    vector<string> augCols = nodeCols;
    augCols.push_back("edgePage");
    augCols.push_back("edgeRowIndex");

    vector<vector<int>> snaRows;
    snaRows.reserve(snRows.size());
    for (auto &nr : snRows) {
        int nid = nr[0];
        auto it = firstPtr.find(nid);
        int ep = -1, er = -1;
        if (it != firstPtr.end()) { ep = it->second.first; er = it->second.second; }
        vector<int> r = nr;
        r.push_back(ep);
        r.push_back(er);
        snaRows.push_back(std::move(r));
    }

    Table *tSNA = new Table(outSNA, augCols);
    tableCatalogue.insertTable(tSNA);
    writeRowsToTable(tSNA, outSNA, snaRows);
}

// ---------- constraint helpers (same as before, no syntax/semantic prints) ----------

static bool parseAttrIndexLoose(const string &name, char prefix, int &outIdx) {
    if (name.size() < 2 || name[0] != prefix) return false;
    for (size_t i = 1; i < name.size(); i++) if (!isdigit(name[i])) return false;
    outIdx = stoi(name.substr(1));
    return true;
}

static bool nodeSatisfies(const vector<bool> &attrs, const vector<pair<int,bool>> &reqs) {
    for (auto &pr : reqs) {
        int idx = pr.first; bool val = pr.second;
        if (idx <= 0 || idx > (int)attrs.size()) return false;
        if (attrs[idx - 1] != val) return false;
    }
    return true;
}

static bool edgeSatisfies(const vector<int> &edgeRow, const vector<pair<int,bool>> &reqs) {
    for (auto &pr : reqs) {
        int idx = pr.first; bool val = pr.second;
        int col = 3 + (idx - 1);
        if (col < 0 || col >= (int)edgeRow.size()) return false;
        bool b = (edgeRow[col] != 0);
        if (b != val) return false;
    }
    return true;
}

// ---------- Dijkstra over full product state with parents (ternary encoding) ----------

struct ParentInfo {
    bool has = false;
    int prevNode = -1;
    int prevCode = 0;
    int edgeIdx = -1;  // index into edgeCache[prevNode]
};

static bool dijkstraTernaryStateWithPath(
    int src, int dest,
    const vector<pair<int,bool>> &nodeReq,
    const vector<pair<int,bool>> &edgeReq,
    const vector<int> &uniformEdgeIdx,
    unordered_map<int, vector<bool>> &nodeCache,
    unordered_map<int, vector<vector<int>>> &edgeCache,
    long long &outDist,
    vector<int> &outNodes,
    vector<vector<int>> &outEdges
) {
    const long long INF = (1LL<<62);
    int k = (int)uniformEdgeIdx.size();

    // ternary powers
    vector<int> pow3(k + 1, 1);
    for (int i = 1; i <= k; i++) {
        // assume k small; if not, just fail in "basic testing" mode
        long long v = 1LL * pow3[i-1] * 3LL;
        if (v > 20000000LL) return false;
        pow3[i] = (int)v;
    }
    int S = pow3[k];

    auto getNodeCached = [&](int id) -> const vector<bool>& {
        auto it = nodeCache.find(id);
        if (it != nodeCache.end()) return it->second;
        nodeCache[id] = getNodeInfo(id);
        return nodeCache[id];
    };
    auto getEdgesCached = [&](int id) -> const vector<vector<int>>& {
        auto it = edgeCache.find(id);
        if (it != edgeCache.end()) return it->second;
        edgeCache[id] = getEdgeInfo(id);
        return edgeCache[id];
    };

    struct PQ { long long d; int u; int code; };
    struct Cmp { bool operator()(PQ const& a, PQ const& b) const { return a.d > b.d; } };

    unordered_map<int, vector<long long>> dist;
    unordered_map<int, vector<ParentInfo>> parent;
    priority_queue<PQ, vector<PQ>, Cmp> pq;

    dist[src] = vector<long long>(S, INF);
    parent[src] = vector<ParentInfo>(S);
    dist[src][0] = 0;
    pq.push({0, src, 0});

    int endCode = -1;

    while (!pq.empty()) {
        auto cur = pq.top(); pq.pop();
        int u = cur.u, code = cur.code;
        long long du = cur.d;

        auto it = dist.find(u);
        if (it == dist.end()) continue;
        if (du != it->second[code]) continue;

        if (u == dest) {
            outDist = du;
            endCode = code;
            break;
        }

        const auto &adj = getEdgesCached(u);
        for (int ei = 0; ei < (int)adj.size(); ei++) {
            const auto &erow = adj[ei];
            if ((int)erow.size() < 3) continue;

            int v = erow[1];
            long long w = (long long)erow[2];
            if (w < 0) continue;
            if (!edgeSatisfies(erow, edgeReq)) continue;

            int newCode = code;
            bool ok = true;
            for (int j = 0; j < k; j++) {
                int bj = uniformEdgeIdx[j]; // 1-based
                int col = 3 + (bj - 1);
                if (col < 0 || col >= (int)erow.size()) { ok = false; break; }

                int wantDigit = (erow[col] != 0) ? 2 : 1; // 2=>1, 1=>0
                int digit = (newCode / pow3[j]) % 3;

                if (digit == 0) newCode += wantDigit * pow3[j];
                else if (digit != wantDigit) { ok = false; break; }
            }
            if (!ok) continue;

            const vector<bool> &vinfo = getNodeCached(v);
            if (vinfo.empty()) continue;
            if (!nodeSatisfies(vinfo, nodeReq)) continue;

            long long nd = du + w;

            if (dist.find(v) == dist.end()) {
                dist[v] = vector<long long>(S, INF);
                parent[v] = vector<ParentInfo>(S);
            }

            if (nd < dist[v][newCode]) {
                dist[v][newCode] = nd;
                parent[v][newCode] = ParentInfo{true, u, code, ei};
                pq.push({nd, v, newCode});
            }
        }
    }

    if (endCode == -1) return false;

    // Reconstruct path
    vector<int> nodesRev;
    vector<vector<int>> edgesRev;

    int curNode = dest;
    int curCode = endCode;

    while (!(curNode == src && curCode == 0)) {
        auto pit = parent.find(curNode);
        if (pit == parent.end()) return false;
        ParentInfo p = pit->second[curCode];
        if (!p.has) return false;

        // edge row is stored in edgeCache[p.prevNode][p.edgeIdx]
        const auto &adjPrev = getEdgesCached(p.prevNode);
        if (p.edgeIdx < 0 || p.edgeIdx >= (int)adjPrev.size()) return false;

        edgesRev.push_back(adjPrev[p.edgeIdx]);
        nodesRev.push_back(curNode);

        curNode = p.prevNode;
        curCode = p.prevCode;
    }
    nodesRev.push_back(src);

    reverse(nodesRev.begin(), nodesRev.end());
    reverse(edgesRev.begin(), edgesRev.end());

    outNodes = std::move(nodesRev);
    outEdges = std::move(edgesRev);
    return true;
}

// ---------- executePATH: prints + saves graph when True ----------

void executePATH() {
    logger.log("executePATH");

    int src = stoi(parsedQuery.src_node);
    int dest = stoi(parsedQuery.dest_node);

    // output graph name is LHS token (RES in "RES <- PATH ...")
    string outGraph = tokenizedQuery[0];

    string inGraph = parsedQuery.loadGraphRelationName;
    string type = (parsedQuery.graphType == DIRECTED) ? "D" : "U";

    // attribute counts from table headers (robust even if result is small)
    Table *inNodesT = tableCatalogue.getTable(inGraph + "_Nodes_" + type);
    Table *inEdgesT = tableCatalogue.getTable(inGraph + "_Edges_" + type);
    if (!inNodesT || !inEdgesT) { cout << "False" << endl; return; }

    int nodeAttrCount = (int)inNodesT->columnCount - 1; // NodeID + A...
    int edgeAttrCount = (int)inEdgesT->columnCount - 3; // Src,Dest,Weight + B...

    unordered_map<int, vector<bool>> nodeCache;
    unordered_map<int, vector<vector<int>>> edgeCache;
    nodeCache.reserve(1024);
    edgeCache.reserve(1024);

    vector<bool> srcInfo = getNodeInfo(src);
    if (srcInfo.empty()) { cout << "Node does not exist" << endl; return; }
    vector<bool> destInfo = getNodeInfo(dest);
    if (destInfo.empty()) { cout << "Node does not exist" << endl; return; }

    nodeCache[src] = srcInfo;
    nodeCache[dest] = destInfo;

    // Collect constraints (no error printing here)
    vector<pair<int,bool>> baseNodeReq;
    vector<pair<int,bool>> baseEdgeReq;
    vector<int> uniformNodeIdx;
    vector<int> uniformEdgeIdx;

    struct AnyCond { bool isNode; bool val; };
    vector<AnyCond> anyConds;

    for (auto &c : parsedQuery.path_condtions) {
        const string &attrName = c.first.first;
        bool isNode = c.first.second;
        int bc = (int)c.second; // 0,1,2

        if (attrName == "ANY") {
            if (bc == 2) { cout << "False" << endl; return; }
            anyConds.push_back({isNode, bc == 1});
            continue;
        }

        if (isNode) {
            int idx = -1;
            if (!parseAttrIndexLoose(attrName, 'A', idx)) { cout << "False" << endl; return; }
            if (idx < 1 || idx > nodeAttrCount) { cout << "False" << endl; return; }
            if (bc == 2) uniformNodeIdx.push_back(idx);
            else baseNodeReq.push_back({idx, bc == 1});
        } else {
            int idx = -1;
            if (!parseAttrIndexLoose(attrName, 'B', idx)) { cout << "False" << endl; return; }
            if (idx < 1 || idx > edgeAttrCount) { cout << "False" << endl; return; }
            if (bc == 2) uniformEdgeIdx.push_back(idx);
            else baseEdgeReq.push_back({idx, bc == 1});
        }
    }

    // Rewrite uniform node Aq(N) => Aq(N)==Aq(src)
    for (int idx : uniformNodeIdx) baseNodeReq.push_back({idx, srcInfo[idx - 1]});

    auto mergeReqs = [](int attrCount, const vector<pair<int,bool>> &reqs, vector<int8_t> &arr) -> bool {
        arr.assign(attrCount + 1, -1);
        for (auto &pr : reqs) {
            int idx = pr.first;
            int8_t v = pr.second ? 1 : 0;
            if (arr[idx] != -1 && arr[idx] != v) return false;
            arr[idx] = v;
        }
        return true;
    };

    vector<int8_t> baseNodeArr, baseEdgeArr;
    if (!mergeReqs(nodeAttrCount, baseNodeReq, baseNodeArr)) { cout << "False" << endl; return; }
    if (!mergeReqs(edgeAttrCount, baseEdgeReq, baseEdgeArr)) { cout << "False" << endl; return; }

    auto arrToPairs = [](const vector<int8_t> &arr) {
        vector<pair<int,bool>> req;
        for (int i = 1; i < (int)arr.size(); i++) if (arr[i] != -1) req.push_back({i, arr[i] == 1});
        return req;
    };

    // ANY candidates (cartesian product if multiple ANY)
    vector<vector<int>> anyCandidates;
    anyCandidates.reserve(anyConds.size());
    for (auto &ac : anyConds) {
        vector<int> cand;
        if (ac.isNode) {
            for (int i = 1; i <= nodeAttrCount; i++) {
                if (srcInfo[i - 1] == ac.val && destInfo[i - 1] == ac.val) cand.push_back(i);
            }
        } else {
            for (int j = 1; j <= edgeAttrCount; j++) cand.push_back(j);
        }
        if (cand.empty()) { cout << "False" << endl; return; }
        anyCandidates.push_back(std::move(cand));
    }

    const long long INF = (1LL<<62);
    long long best = INF;
    bool found = false;
    vector<int> bestNodes;
    vector<vector<int>> bestEdges;

    function<void(int, vector<int8_t>, vector<int8_t>)> dfs =
        [&](int pos, vector<int8_t> nodeArr, vector<int8_t> edgeArr) {
            if (pos == (int)anyConds.size()) {
                vector<pair<int,bool>> nodeReq = arrToPairs(nodeArr);
                vector<pair<int,bool>> edgeReq = arrToPairs(edgeArr);

                if (!nodeSatisfies(srcInfo, nodeReq)) return;
                if (!nodeSatisfies(destInfo, nodeReq)) return;

                if (src == dest) {
                    if (0 < best) {
                        best = 0;
                        found = true;
                        bestNodes = {src};
                        bestEdges.clear();
                    }
                    return;
                }

                long long d = 0;
                vector<int> pNodes;
                vector<vector<int>> pEdges;

                bool ok = dijkstraTernaryStateWithPath(
                    src, dest, nodeReq, edgeReq, uniformEdgeIdx,
                    nodeCache, edgeCache, d, pNodes, pEdges
                );

                if (ok && d < best) {
                    best = d;
                    found = true;
                    bestNodes = std::move(pNodes);
                    bestEdges = std::move(pEdges);
                }
                return;
            }

            const auto &ac = anyConds[pos];
            for (int idx : anyCandidates[pos]) {
                int8_t want = ac.val ? 1 : 0;
                if (ac.isNode) {
                    if (nodeArr[idx] != -1 && nodeArr[idx] != want) continue;
                    auto nodeArr2 = nodeArr;
                    nodeArr2[idx] = want;
                    dfs(pos + 1, std::move(nodeArr2), edgeArr);
                } else {
                    if (edgeArr[idx] != -1 && edgeArr[idx] != want) continue;
                    auto edgeArr2 = edgeArr;
                    edgeArr2[idx] = want;
                    dfs(pos + 1, nodeArr, std::move(edgeArr2));
                }
            }
        };

    dfs(0, baseNodeArr, baseEdgeArr);

    if (!found) {
        cout << "False" << endl;
        return;
    }

    cout << "True " << best << endl;

    // Save result graph tables (Nodes/Edges + SN/SE + SN_Augmented)
    materializePathGraph(
        outGraph, type, inGraph,
        bestNodes, bestEdges,
        nodeAttrCount, edgeAttrCount,
        nodeCache
    );
}
