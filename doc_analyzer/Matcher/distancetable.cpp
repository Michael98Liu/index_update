#include "distancetable.h"

#include <algorithm>
#include <vector>
#include <map>
#include <iostream>

#include "graph.h"
#include "block.h"

using namespace std;

DistanceTable::DistanceTable(int blocklimit, BlockGraph& graph, vector<Block*>& toporder) : maxsteps(blocklimit) {
    //Initialize all vertices in graph
    //Since order doesn't matter, use toporder
    for(Block* vertex : toporder) {
        this->initVertex(vertex);
    }
    
    //Fill out the dist list for each vertex
    for(Block* vertex : toporder) {
        vector<Block*> adjacencylist = graph.getAdjacencyList(vertex);
        for(Block* neighbor : adjacencylist) {
            this->mergeIntoNext(vertex, neighbor);
        }
    }
}

vector<pair<int, Block*>> DistanceTable::findAllBestPaths() {
    //Initialize our vector to fit the results
    //The actual list may be shorter than maxsteps if that graph can be traversed using
    //less than "maxsteps" hops
    vector<pair<int, Block*>> bestlist;
    bestlist.resize(maxsteps, make_pair(0, nullptr));
    
    //find the longest distance
    for(auto iter = table.begin(); iter != table.end(); iter++) {
        cout << (iter->first)->oldloc << "-" << (iter->first)->newloc << endl;
        
        vector<pair<int, Block*>> testlist = iter->second;
        for(size_t i = 0; i < testlist.size(); i++) {
            if(testlist[i].second != nullptr)
                cout << "\t" << testlist[i].first << " " << (testlist[i].second)->oldloc << "-" << (testlist[i].second)->newloc << endl;
            else
                cout << "\t" << testlist[i].first << " S" << endl;
            if(testlist[i].first > bestlist[i].first) {
                //The list should contain the ending vertex, not the prev vertex
                bestlist[i].first = testlist[i].first;
                bestlist[i].second = iter->first;
            }
        }
    }
    
    //Trim any null pairs remaining
    while(bestlist.back().second == nullptr)
        bestlist.pop_back();
    
    return bestlist;
}

pair<int, Block*> DistanceTable::findOptimalPath(int a) {
    auto bestlist = findAllBestPaths();
    
    int prevtotalweight = 0;
    for(size_t i = 0; i < bestlist.size(); ++i) {
        int curtotalweight = bestlist[i].first;
        if(curtotalweight == 0) break;
        
        int margin = curtotalweight - prevtotalweight;
        //ax - y > 0?
        //If there isn't enough saved common text to take the current pair, then return the previous one
        if(margin < a*(i+1))
            return bestlist[i-1];
        
        prevtotalweight = curtotalweight;
    }
    
    return bestlist.back();
}

vector<Block*> DistanceTable::tracePath(Block* ending, int steps) {
    vector<Block*> path;
    if(ending == nullptr)
        return path;
    
    path.push_back(ending);
    steps--;
    Block* next = ending;
    //steps must be strictly greater than 0
    //Since we are pushing the "next" vertex, we would end up pushing nullptr
    //if we let steps=0
    while(steps > 0) {
        next = table[next][steps].second;
        path.push_back(next);
        steps--;
    }
    
    reverse(path.begin(), path.end());
    
    return path;
}

void DistanceTable::mergeIntoNext(Block* prev, Block* next) {
    int weight = next->run.size();
    
    //If neighbor's distlist is not large enough for comparing, resize it
    if(table[next].size() < table[prev].size()+1)
        table[next].resize(table[prev].size()+1, make_pair(0, nullptr));

    //Compare each entry in prev to the entry in next+1
    for(size_t i = 0; i < table[prev].size(); i++) {
        if(table[prev][i].first == 0)
            continue;
        
        if(table[prev][i].first + weight > table[next][i+1].first)
        {
            table[next][i+1].first = table[prev][i].first + weight;
            table[next][i+1].second = prev;
        }
    }

    //If the table exceeds the number of steps, remove the extra entry
    if(table[next].size() > maxsteps)
        table[next].pop_back();
}

void DistanceTable::initVertex(Block* V) {
    if(table.find(V) == table.end()) {
        vector<pair<int, Block*>> initdist;
        //We can assume that nullptr refers to the source node
        initdist.emplace_back(V->run.size(), V);
        table[V] = initdist;
    }
}