#include "global.h"

/**
 * @brief Construct a new Table:: Table object
 *
 */
Table::Table()
{
    logger.log("Table::Table");
}

/**
 * @brief Construct a new Table:: Table object used in the case where the data
 * file is available and LOAD command has been called. This command should be
 * followed by calling the load function;
 *
 * @param tableName 
 */
Table::Table(string tableName)
{
    logger.log("Table::Table");
    this->sourceFileName = "../data/" + tableName + ".csv";
    this->tableName = tableName;
}

/**
 * @brief Construct a new Dummy Table to add to later Table:: Table object used in the case where the data
 *
 * @param tableName 
 * @param source
 */

Table::Table(string tableName, string source)
{
    logger.log("Table::Table");
    this->sourceFileName = "../data/" + source + ".csv";
    this->tableName = tableName;
}

/**
 * @brief Construct a new Table:: Table object used when an assignment command
 * is encountered. To create the table object both the table name and the
 * columns the table holds should be specified.
 *
 * @param tableName 
 * @param columns 
 */
Table::Table(string tableName, vector<string> columns)
{
    logger.log("Table::Table");
    this->sourceFileName = "../data/temp/" + tableName + ".csv";
    this->tableName = tableName;
    this->columns = columns;
    this->columnCount = columns.size();
    this->maxRowsPerBlock = (uint)((BLOCK_SIZE * 1000) / (sizeof(int) * columnCount));
    this->writeRow<string>(columns);
}


/**
 * @brief The load function is used when the LOAD command is encountered. It
 * reads data from the source file, splits it into blocks and updates table
 * statistics.
 *
 * @return true if the table has been successfully loaded 
 * @return false if an error occurred 
 */
bool Table::load()
{
    logger.log("Table::load");
    fstream fin(this->sourceFileName, ios::in);
    string line;
    if (getline(fin, line))
    {
        fin.close();
        if (this->extractColumnNames(line))
            if (this->blockify())
                return true;
    }
    fin.close();
    return false;
}



/**
 * @brief Function extracts column names from the header line of the .csv data
 * file. 
 *
 * @param line 
 * @return true if column names successfully extracted (i.e. no column name
 * repeats)
 * @return false otherwise
 */
bool Table::extractColumnNames(string firstLine)
{
    logger.log("Table::extractColumnNames");
    unordered_set<string> columnNames;
    string word;
    stringstream s(firstLine);
    while (getline(s, word, ','))
    {
        word.erase(std::remove_if(word.begin(), word.end(), ::isspace), word.end());
        if (columnNames.count(word))
            return false;
        columnNames.insert(word);
        this->columns.emplace_back(word);
    }
    this->columnCount = this->columns.size();
    this->maxRowsPerBlock = (uint)((BLOCK_SIZE * 1000) / (sizeof(int) * this->columnCount));
    return true;
}

/**
 * @brief This function splits all the rows and stores them in multiple files of
 * one block size. 
 *
 * @return true if successfully blockified
 * @return false otherwise
 */
bool Table::blockify()
{
    logger.log("Table::blockify");
    ifstream fin(this->sourceFileName, ios::in);
    string line, word;
    vector<int> row(this->columnCount, 0);
    vector<vector<int>> rowsInPage(this->maxRowsPerBlock, row);
    int pageCounter = 0;
    unordered_set<int> dummy;
    dummy.clear();
    this->distinctValuesInColumns.assign(this->columnCount, dummy);
    this->distinctValuesPerColumnCount.assign(this->columnCount, 0);
    getline(fin, line);
    while (getline(fin, line))
    {
        stringstream s(line);
        for (int columnCounter = 0; columnCounter < this->columnCount; columnCounter++)
        {
            if (!getline(s, word, ','))
                return false;
            row[columnCounter] = stoi(word);
            rowsInPage[pageCounter][columnCounter] = row[columnCounter];
        }
        pageCounter++;
        this->updateStatistics(row);
        if (pageCounter == this->maxRowsPerBlock)
        {
            bufferManager.writePage(this->tableName, this->blockCount, rowsInPage, pageCounter);
            this->blockCount++;
            this->rowsPerBlockCount.emplace_back(pageCounter);
            pageCounter = 0;
        }
    }
    if (pageCounter)
    {
        bufferManager.writePage(this->tableName, this->blockCount, rowsInPage, pageCounter);
        this->blockCount++;
        this->rowsPerBlockCount.emplace_back(pageCounter);
        pageCounter = 0;
    }

    if (this->rowCount == 0)
        return false;
    this->distinctValuesInColumns.clear();
    return true;
}

/**
 * @brief Given a row of values, this function will update the statistics it
 * stores i.e. it updates the number of rows that are present in the column and
 * the number of distinct values present in each column. These statistics are to
 * be used during optimisation.
 *
 * @param row 
 */
void Table::updateStatistics(vector<int> row)
{
    this->rowCount++;
    for (int columnCounter = 0; columnCounter < this->columnCount; columnCounter++)
    {
        if (!this->distinctValuesInColumns[columnCounter].count(row[columnCounter]))
        {
            this->distinctValuesInColumns[columnCounter].insert(row[columnCounter]);
            this->distinctValuesPerColumnCount[columnCounter]++;
        }
    }
}

/**
 * @brief Checks if the given column is present in this table.
 *
 * @param columnName 
 * @return true 
 * @return false 
 */
bool Table::isColumn(string columnName)
{
    logger.log("Table::isColumn");
    for (auto col : this->columns)
    {
        if (col == columnName)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Renames the column indicated by fromColumnName to toColumnName. It is
 * assumed that checks such as the existence of fromColumnName and the non prior
 * existence of toColumnName are done.
 *
 * @param fromColumnName 
 * @param toColumnName 
 */
void Table::renameColumn(string fromColumnName, string toColumnName)
{
    logger.log("Table::renameColumn");
    for (int columnCounter = 0; columnCounter < this->columnCount; columnCounter++)
    {
        if (columns[columnCounter] == fromColumnName)
        {
            columns[columnCounter] = toColumnName;
            break;
        }
    }
    return;
}

/**
 * @brief Function prints the first few rows of the table. If the table contains
 * more rows than PRINT_COUNT, exactly PRINT_COUNT rows are printed, else all
 * the rows are printed.
 *
 */
void Table::print()
{
    logger.log("Table::print");
    uint count = min((long long)PRINT_COUNT, this->rowCount);

    //print headings
    this->writeRow(this->columns, cout);

    Cursor cursor(this->tableName, 0);
    vector<int> row;
    for (int rowCounter = 0; rowCounter < count; rowCounter++)
    {
        row = cursor.getNext();
        this->writeRow(row, cout);
    }
    printRowCount(this->rowCount);
}



/**
 * @brief This function returns one row of the table using the cursor object. It
 * returns an empty row is all rows have been read.
 *
 * @param cursor 
 * @return vector<int> 
 */
void Table::getNextPage(Cursor *cursor)
{
    logger.log("Table::getNext");

        if (cursor->pageIndex < this->blockCount - 1)
        {
            cursor->nextPage(cursor->pageIndex+1);
        }
}



/**
 * @brief called when EXPORT command is invoked to move source file to "data"
 * folder.
 *
 */
void Table::makePermanent()
{
    logger.log("Table::makePermanent");
    if(!this->isPermanent())
        bufferManager.deleteFile(this->sourceFileName);
    string newSourceFile = "../data/" + this->tableName + ".csv";
    ofstream fout(newSourceFile, ios::out);

    //print headings
    this->writeRow(this->columns, fout);

    Cursor cursor(this->tableName, 0);
    vector<int> row;
    for (int rowCounter = 0; rowCounter < this->rowCount; rowCounter++)
    {
        row = cursor.getNext();
        this->writeRow(row, fout);
    }
    fout.close();
}

/**
 * @brief Function to check if table is already exported
 *
 * @return true if exported
 * @return false otherwise
 */
bool Table::isPermanent()
{
    logger.log("Table::isPermanent");
    if (this->sourceFileName == "../data/" + this->tableName + ".csv")
    return true;
    return false;
}

/**
 * @brief The unload function removes the table from the database by deleting
 * all temporary files created as part of this table
 *
 */
void Table::unload(){
    logger.log("Table::~unload");
    for (int pageCounter = 0; pageCounter < this->blockCount; pageCounter++)
        bufferManager.deleteFile(this->tableName, pageCounter);
    if (!isPermanent())
    {
        if (this->sourceFileName.find("/temp/") != string::npos) 
        {
            bufferManager.deleteFile(this->sourceFileName);
        }
    }
}

/**
 * @brief Function that returns a cursor that reads rows from this table
 * 
 * @return Cursor 
 */
Cursor Table::getCursor()
{
    logger.log("Table::getCursor");
    Cursor cursor(this->tableName, 0);
    return cursor;
}
/**
 * @brief Function that returns the index of column indicated by columnName
 * 
 * @param columnName 
 * @return int 
 */
int Table::getColumnIndex(string columnName)
{
    logger.log("Table::getColumnIndex");
    for (int columnCounter = 0; columnCounter < this->columnCount; columnCounter++)
    {
        if (this->columns[columnCounter] == columnName)
            return columnCounter;
    }
}



namespace fs = filesystem;

int countPages(string tableName) {
    string path = "../data/temp/";
    string targetPrefix = tableName + "_Page";
    int pageCount = 0;

    for (const auto& file : fs::directory_iterator(path)) {
        if (file.path().filename().string().find(targetPrefix) == 0) {
            pageCount++;
        }
    }
    return pageCount;
}

void Table::Merge(int leftIndex, int rightIndex, int runSize, string outputTableName, int &outputPageIndex)
{
    int totalPages = countPages(this->tableName);
    int leftEnd = min(leftIndex + runSize, rightIndex); 
    int rightEnd = min(rightIndex + runSize, totalPages); 

    Table* outTable = tableCatalogue.getTable(outputTableName);

    // Current page pointers (RAM slots)
    int p1 = leftIndex;
    int p2 = rightIndex;

    // Load Initial Pages
    Page page1 = (p1 < leftEnd) ? bufferManager.getPage(this->tableName, p1) : Page();
    Page page2 = (p2 < rightEnd) ? bufferManager.getPage(this->tableName, p2) : Page();

    int r1 = 0; 
    int r2 = 0;
    
    // Pre-fetch the first rows. If the page is empty or invalid, these will be empty.
    vector<int> nextRow1 = (p1 < leftEnd) ? page1.getRow(r1) : vector<int>();
    vector<int> nextRow2 = (p2 < rightEnd) ? page2.getRow(r2) : vector<int>();

    vector<vector<int>> outputBuffer; 

    // LOOP: While BOTH runs still have data
    while (!nextRow1.empty() && !nextRow2.empty())
    {
        // Compare
        if (nextRow1 <= nextRow2) {
            outputBuffer.push_back(nextRow1);
            r1++;
            nextRow1 = page1.getRow(r1); // Fetch next row

            // If page is exhausted (row is empty), try to load next page
            if (nextRow1.empty()) {
                p1++;
                if (p1 < leftEnd) {
                    page1 = bufferManager.getPage(this->tableName, p1);
                    r1 = 0;
                    nextRow1 = page1.getRow(r1);
                }
            }
        } else {
            outputBuffer.push_back(nextRow2);
            r2++;
            nextRow2 = page2.getRow(r2); // Fetch next row

            // If page is exhausted
            if (nextRow2.empty()) {
                p2++;
                if (p2 < rightEnd) {
                    page2 = bufferManager.getPage(this->tableName, p2);
                    r2 = 0;
                    nextRow2 = page2.getRow(r2);
                }
            }
        }
        
        // Flush to Disk if Buffer is Full
        if (outputBuffer.size() >= this->maxRowsPerBlock) {
            bufferManager.writePage(outputTableName, outputPageIndex++, outputBuffer, outputBuffer.size());
            outTable->rowsPerBlockCount.push_back(outputBuffer.size());
            outTable->rowCount += outputBuffer.size(); 
            outTable->blockCount++;
            outputBuffer.clear();
        }
    }
    
    // --- CLEANUP LEFT RUN ---
    while (!nextRow1.empty()) {
        outputBuffer.push_back(nextRow1);
        r1++;
        nextRow1 = page1.getRow(r1);
        
        if (nextRow1.empty()) {
            p1++;
            if (p1 < leftEnd) {
                page1 = bufferManager.getPage(this->tableName, p1);
                r1 = 0;
                nextRow1 = page1.getRow(r1);
            }
        }
        if (outputBuffer.size() >= this->maxRowsPerBlock) {
            bufferManager.writePage(outputTableName, outputPageIndex++, outputBuffer, outputBuffer.size());
            outTable->rowsPerBlockCount.push_back(outputBuffer.size());
            outTable->rowCount += outputBuffer.size();
            outTable->blockCount++;
            outputBuffer.clear();
        }
    }
    
    // --- CLEANUP RIGHT RUN ---
    while (!nextRow2.empty()) {
        outputBuffer.push_back(nextRow2);
        r2++;
        nextRow2 = page2.getRow(r2);
        
        if (nextRow2.empty()) {
            p2++;
            if (p2 < rightEnd) {
                page2 = bufferManager.getPage(this->tableName, p2);
                r2 = 0;
                nextRow2 = page2.getRow(r2);
            }
        }
        if (outputBuffer.size() >= this->maxRowsPerBlock) {
            bufferManager.writePage(outputTableName, outputPageIndex++, outputBuffer, outputBuffer.size());
            outTable->rowsPerBlockCount.push_back(outputBuffer.size());
            outTable->rowCount += outputBuffer.size();
            outTable->blockCount++;
            outputBuffer.clear();
        }
    }
    
    // Final Flush
    if (!outputBuffer.empty()) {
        bufferManager.writePage(outputTableName, outputPageIndex++, outputBuffer, outputBuffer.size());

        outTable->rowsPerBlockCount.push_back(outputBuffer.size());
        outTable->rowCount += outputBuffer.size();
        outTable->blockCount++;
    }
}

void Table::MergePass(int runSize)
{
    
    int TotalPages = countPages(this->tableName);
    
    int OutputPageIndex = 0;
    
    string tempTableName = this->tableName + "_temp_" + to_string(runSize);
    
    Table *tempTable = new Table(tempTableName, this->columns);
    tableCatalogue.insertTable(tempTable);

    for(int i = 0; i < TotalPages; i+= 2 * runSize)
    {
        int leftIndex = i;
        int rightIndex = min(i + runSize , TotalPages);
        
        if (rightIndex < TotalPages) {
            Merge(leftIndex, rightIndex, runSize, tempTableName, OutputPageIndex);
        } else {
            Merge(leftIndex, TotalPages, runSize, tempTableName, OutputPageIndex);
        }
        
    }

    for (int i = 0; i < OutputPageIndex; i++)
    {
        // Read from Temp
        Page tempPage = bufferManager.getPage(tempTableName, i);
        
        vector<vector<int>> rows;
        int rowIndex = 0;
        
        // Extract rows
        while(true)
        {
            vector<int> row = tempPage.getRow(rowIndex++);
            if (row.empty()) break;
            rows.push_back(row);
        }

        // Overwrite Main Table
        bufferManager.writePage(this->tableName, i, rows, rows.size());
    }

    for (int i = OutputPageIndex; i < TotalPages; i++)
    {
        bufferManager.deleteFile(this->tableName, i);
    }

    this->rowsPerBlockCount = tempTable->rowsPerBlockCount;
    this->rowCount = tempTable->rowCount;
    this->blockCount = tempTable->blockCount;
 
    // Remove the temp table from the Catalogue (this also frees the memory)
    tableCatalogue.deleteTable(tempTableName);

}

/**
 * @brief The loadgraph function is used when the LOAD GRAPH command is encountered.
 * One will load the nodes in a sorted fashion. this table will additionally store the degree and the offset
 * of the outgoing edges of the vertex in the sorted edge table. The edge table stores the edges sorted by the
 * tuple.
 * 1) Load in the node ids upto one page
 * 2) sort and write to temp files
 */

void Table::GraphSortPass0()
{
    logger.log("Table::loadGraph");
    int PageIndex = 0;
    int TotalPage = countPages(this->tableName);
    
    while(PageIndex < TotalPage)
    {
        vector<vector<int>> rows;
        Page Page1 = bufferManager.getPage(this->tableName, PageIndex);
        int rowIndex = 0;
        while(true)
        {
            vector<int> row = Page1.getRow(rowIndex++);
            if(row.empty())
            {
                break;
            }
            rows.push_back(row);
        }
        sort(rows.begin(), rows.end());

        bufferManager.writePage(this->tableName, PageIndex, rows, rows.size());
        bufferManager.evictFromPool(this->tableName, PageIndex);
        PageIndex++;
    }

    //merge pages recursively?
    int runSize = 1;

    while(runSize < TotalPage)
    {
        logger.log("Merging sorted page set of size " + to_string(runSize));

        MergePass(runSize);

        runSize = 2 * runSize;

        TotalPage = countPages(this->tableName);
    }

}

/**
 * @brief Construct a new Graph:: Graph object used when a graph is provided
 * requires 2 params, name and type
 *
 * @param graphName 
 * @param graphType
 */
Graph::Graph(string graphName, string graphType)
{
    logger.log("Graph::Graph");

    this->GraphName = graphName; 
    this->graphType = graphType;

    this->sourceNodeName = graphName + "_Nodes_" + graphType;
    this->sourceEdgeName = graphName + "_Edges_" + graphType;

}

void Graph::load()
{
    logger.log("Graph::load");

    this->NodesTable = new Table(this->sourceNodeName);
    this->EdgesTable = new Table(this->sourceEdgeName);

    if (this->NodesTable->load())
    {
        tableCatalogue.insertTable(this->NodesTable);
        cout << "Loaded Nodes Table. Column Count: " << this->NodesTable->columnCount << " Row Count: " << this->NodesTable->rowCount << endl;
    }
    if (this->EdgesTable->load())
    {
        tableCatalogue.insertTable(this->EdgesTable);
        cout << "Loaded Edges Table. Column Count: " << this->EdgesTable->columnCount << " Row Count: " << this->EdgesTable->rowCount << endl;
    }

    string SortedNodesRelationName = this->GraphName + "_SN_" + this->graphType;
    string SortedEdgesRelationName = this->GraphName + "_SE_" + this->graphType;

    this->SortedNodesTable = new Table(SortedNodesRelationName, this->sourceNodeName);
    this->SortedEdgesTable = new Table(SortedEdgesRelationName, this->sourceEdgeName);

    if (this->SortedNodesTable->load())
    {
        tableCatalogue.insertTable(this->SortedNodesTable);
        cout << "Loaded UNSORTED Nodes Table. Column Count: " << this->SortedNodesTable->columnCount << " Row Count: " << this->SortedNodesTable->rowCount << endl;
    }
    if (this->SortedEdgesTable->load())
    {
        tableCatalogue.insertTable(this->SortedEdgesTable);
        cout << "Loaded UNSORTED Edges Table. Column Count: " << this->SortedEdgesTable->columnCount << " Row Count: " << this->SortedEdgesTable->rowCount << endl;
    }

    SortedNodesTable->GraphSortPass0();
    SortedEdgesTable->GraphSortPass0();

    // 2. Prepare for Augmented Table Creation
    // New Columns: [OriginalCols..., edgePage, edgeRowIndex]
    string augmentedNodesName = this->GraphName + "_SN_Augmented_" + this->graphType;
    vector<string> newColumns = this->SortedNodesTable->columns;
    newColumns.push_back("edgePage");
    newColumns.push_back("edgeRowIndex");

    Table* augmentedNodesTable = new Table(augmentedNodesName, newColumns);
    tableCatalogue.insertTable(augmentedNodesTable);

    // 3. Simultaneous Scan (Merge Join strategy)
    Cursor nodeCursor = SortedNodesTable->getCursor();
    Cursor edgeCursor = SortedEdgesTable->getCursor();

    vector<int> nodeRow = nodeCursor.getNext();
    
    // Pre-fetch the first edge and track its position
    int currEdgePage = edgeCursor.pageIndex;
    int currEdgeRow = edgeCursor.pagePointer; 
    vector<int> edgeRow = edgeCursor.getNext();

    vector<vector<int>> pageBuffer;
    int outputPageIndex = 0;

    while (!nodeRow.empty()) {
        int nodeId = nodeRow[0];
        int foundPage = -1;
        int foundRow = -1;

        // Advance edge cursor until we find an edge >= current nodeId
        while (!edgeRow.empty() && edgeRow[0] < nodeId) {
            // Track position BEFORE reading next
            currEdgePage = edgeCursor.pageIndex;
            currEdgeRow = edgeCursor.pagePointer;
            edgeRow = edgeCursor.getNext();
        }

        // Check if we matched the node
        if (!edgeRow.empty() && edgeRow[0] == nodeId) {
            // The `currEdgePage` and `currEdgeRow` captured BEFORE the getNext() 
            // call that retrieved this row are the correct indices.
            foundPage = currEdgePage;
            foundRow = currEdgeRow;
        }

        // Construct Augmented Row
        vector<int> newRow = nodeRow;
        newRow.push_back(foundPage);
        newRow.push_back(foundRow);
        pageBuffer.push_back(newRow);

        // Write to Disk if buffer full
        if (pageBuffer.size() >= augmentedNodesTable->maxRowsPerBlock) {
            bufferManager.writePage(augmentedNodesName, outputPageIndex++, pageBuffer, pageBuffer.size());
            augmentedNodesTable->rowsPerBlockCount.push_back(pageBuffer.size());
            augmentedNodesTable->rowCount += pageBuffer.size();
            augmentedNodesTable->blockCount++;
            pageBuffer.clear();
        }

        nodeRow = nodeCursor.getNext();
    }

    // Final Flush
    if (!pageBuffer.empty()) {
        bufferManager.writePage(augmentedNodesName, outputPageIndex++, pageBuffer, pageBuffer.size());
        augmentedNodesTable->rowsPerBlockCount.push_back(pageBuffer.size());
        augmentedNodesTable->rowCount += pageBuffer.size();
        augmentedNodesTable->blockCount++;
    }

    // 4. Update Graph pointer
    // We keep the old table in catalogue but update our main pointer to the new one
    this->SortedNodesTable = augmentedNodesTable;

    cout << "Graph Loaded. Nodes augmented with Edge pointers (Page:Row)." << endl;
}
    
