#include "analyzer.h"

#include <vector>
#include <utility>

#include "Matcher/matcher.h"
#include "Matcher/stringencoder.h"
#include "Matcher/translate.h"
#include "externalpostings.h"
#include "Structures/documentstore.h"
#include "Structures/translationtable.h"

#define MIN_BLOCK_SIZE 10
#define MAX_BLOCK_COUNT 20

using namespace std;

//Assumed this is called from the index when a new document arrives
MatcherInfo indexUpdate(string& url, string& newpage, string& timestamp, Structures::DocumentStore& docstore, Structures::TranslationTable& transtable) {
    //-fetch the previous version, and the did of the document, from a tuple store or database (TBD)
    Structures::DocumentTuple olddoc = docstore.getDocument(url);
    
    //-call makePosts(URL, did, currentpage, previouspage), which generates and returns the new postings that you are creating by your matching algorithm (that is, non-positional and position postings) and the additional translation statements to be appended.
    MatcherInfo info = makePosts(olddoc, newpage);

    //-now you can directly call Fengyuan's code to insert those posts(to be done later)

    //-and then append the translation commands to the right translation vector
    transtable.insert(info.translations, olddoc.docID);

    //-and store the currentpage instead of the previouspage in the tuple store.
    docstore.insertDocument(url, newpage, info.maxfragID, timestamp);

    return info;
}

MatcherInfo makePosts(Structures::DocumentTuple& olddoc, string& newpage) {
    //-check if there was a previous version, if not create postings with fragid = 0
    int fragID = olddoc.maxfragID;
    
    Matcher::StringEncoder se(olddoc.doc, newpage);

    //-else, run the graph based matching algorithm on the two versions
    vector<shared_ptr<Matcher::Block>> commonblocks = Matcher::getOptimalBlocks(se, MIN_BLOCK_SIZE, MAX_BLOCK_COUNT, 5);
    
    //Get the translation and posting list
    vector<Matcher::Translation> translist = Matcher::getTranslations(se.getOldSize(), se.getNewSize(), commonblocks);
    auto postingslist = Matcher::getPostings(commonblocks, olddoc.docID, fragID, se);
    
    //Number of fragIDs used is exactly proportional to the number of positional postings inserted
    fragID += postingslist.second.size();

    //-generate postings and translation statements, and return them. (Question: how do we know the previous largest fragid for this document, so we know what to use as the next fragid? Maybe store with did in the tuple store?)
    MatcherInfo posts(postingslist.first, postingslist.second, translist, fragID);
    
    return posts;
}
