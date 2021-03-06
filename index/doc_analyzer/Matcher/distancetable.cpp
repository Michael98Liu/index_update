#include "distancetable.h"

#include <algorithm>
#include <vector>
#include <map>
#include <iostream>

#include "graph.h"
#include "block.h"

using namespace std;

namespace Matcher {
    DistanceTable::DistanceTable(int blocklimit, BlockGraph& graph, vector<shared_ptr<Block>>& toporder) : maxsteps(blocklimit) {
        //Initialize all vertices in graph
        //Since order doesn't matter, use toporder
        for(shared_ptr<Block> vertex : toporder) {
            this->initVertex(vertex);
        }
        
        //Fill out the dist list for each vertex
        for(shared_ptr<Block> vertex : toporder) {
            vector<shared_ptr<Block>> adjacencylist = graph.getAdjacencyList(vertex);
            for(shared_ptr<Block> neighbor : adjacencylist) {
                this->mergeIntoNext(vertex, neighbor);
            }
        }
    }
    
    vector<DistanceTable::TableEntry> DistanceTable::findAllBestPaths() {
        //Initialize our vector to fit the results
        //The actual list may be shorter than maxsteps if that graph can be traversed using
        //less than "maxsteps" hops
        vector<DistanceTable::TableEntry> bestlist;
        bestlist.resize(maxsteps, DistanceTable::TableEntry(-1, -1, nullptr, nullptr));
        
        //find the longest distance
        for(auto iter = tablelist.begin(); iter != tablelist.end(); iter++) {        
            vector<DistanceTable::TableEntry> testtable = iter->second;
            for(size_t i = 0; i < testtable.size(); i++) {;
                if(testtable[i].distance > bestlist[i].distance) {
                    bestlist[i] = testtable[i];
                }
            }
        }
        
        //Trim any null pairs remaining, but don't trim if the list is empty
        while(bestlist.back().current == nullptr && bestlist.size() > 0)
            bestlist.pop_back();
        
        return bestlist;
    }
    
    vector<shared_ptr<Block>> DistanceTable::findOptimalPath(int a) {
        vector<DistanceTable::TableEntry> bestlist = findAllBestPaths();
        //int refers to steps, not weight
        DistanceTable::TableEntry bestending(-1, -1, nullptr, nullptr);
        
        int prevtotalweight = 0;
        for(size_t i = 0; i < bestlist.size(); ++i) {
            int curtotalweight = bestlist[i].distance;
            if(curtotalweight == 0) break;
            
            int margin = curtotalweight - prevtotalweight;
            //ax - y > 0?
            //If there isn't enough saved common text to take the next pair, then return the current one
            if(margin < a*(i+1)) {
                //Number of steps is one greater than the index
                bestending = bestlist[i];
                break;
            }
            
            prevtotalweight = curtotalweight;
        }
        
        if(bestending.current == nullptr && !bestlist.empty())
            bestending = bestlist.back();
        
        return tracePath(bestending);
    }
    
    vector<shared_ptr<Block>> DistanceTable::tracePath(DistanceTable::TableEntry ending) {
        vector<shared_ptr<Block>> path;
        if(ending.current == nullptr)
            return path;
        
        path.push_back(ending.current);
        DistanceTable::TableEntry candidate = ending;
        
        while(candidate.prev != nullptr) {
            path.push_back(candidate.prev);
            candidate = getPreviousEntry(candidate);
        }
        
        reverse(path.begin(), path.end());
        
        return path;
    }
    
    void DistanceTable::mergeIntoNext(shared_ptr<Block> prev, shared_ptr<Block> next) {
        int weight = next->run.size();
        
        //If neighbor's distlist is not large enough for comparing, resize it
        if(tablelist[next].size() < tablelist[prev].size()+1)
            tablelist[next].resize(tablelist[prev].size()+1, DistanceTable::TableEntry(-1, -1, nullptr, nullptr));
    
        //Compare each entry in prev to the entry in next+1
        for(size_t i = 0; i < tablelist[prev].size(); i++) {
            if(tablelist[prev][i].current == nullptr)
                continue;
            
            if(tablelist[prev][i].distance + weight > tablelist[next][i+1].distance) {
                tablelist[next][i+1].steps = tablelist[prev][i].steps + 1;
                tablelist[next][i+1].distance = tablelist[prev][i].distance + weight;
                tablelist[next][i+1].current = next;
                tablelist[next][i+1].prev = prev;
            }
        }
    
        //If the table exceeds the number of steps, remove the extra entry
        if(tablelist[next].size() > maxsteps)
            tablelist[next].pop_back();
    }
    
    DistanceTable::TableEntry DistanceTable::getPreviousEntry(DistanceTable::TableEntry te) {
        if(te.steps < 2)
            return DistanceTable::TableEntry(-1, -1, nullptr, nullptr);
        //subtract one to offset step/index difference, subtract another one to get previous step
        return tablelist[te.prev][te.steps-2];
    }
    
    void DistanceTable::initVertex(shared_ptr<Block> V) {
        if(tablelist.find(V) == tablelist.end()) {
            vector<DistanceTable::TableEntry> tableinit;
            tableinit.push_back(DistanceTable::TableEntry(1, V->run.size(), V, nullptr));
            //Assume that nullptr refers to the source node
            tablelist[V] = tableinit;
        }
    }
}
