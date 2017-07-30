#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <map>
#include <string>
#include <dirent.h>
#include "reader.hpp"

#define NO_DOC 10 //temporary use
#define POSTING_LIMIT 1000 //make sure doesn't exceed memory limit
#define INDEX "./test_data/compressedIndex"
#define INFO "./test_data/docInfo"
#define PDIR "./disk_index/positional/"

using namespace std;

class Posting{
public:
	Posting (){
	};

	Posting(unsigned int id, unsigned int d, unsigned int f = 0, unsigned int p = 0){
		termID = id;
		docID = d;
		fragID = f;
		pos = p;
	}

	friend bool operator< (Posting p1, Posting p2){
		if(p1.termID == p2.termID){
        	if(p1.docID == p2.docID){
        		if(p1.fragID == p2.fragID){
        			return (p1.pos < p2.pos);
        		}else{
        			return (p1.fragID < p2.fragID);
        		}
        	}else{
        		return (p1.docID < p2.docID);
        	}
        }else{
        	return (p1.termID < p2.termID);
        }
	}

	friend bool operator> (Posting p1, Posting p2){
		return !(p1 < p2);
	}

	friend bool operator== (Posting p1, Posting p2){
		if(p1.termID == p2.termID && p1.docID == p2.docID && p1.fragID == p2.fragID && p1.pos == p2.pos) return true;
		else return false;
	}

	unsigned int termID;
	unsigned int docID;
	unsigned int fragID;
	unsigned int pos;
};

struct fileinfo{//a file that contains a part (or whole) postinglist
	string filename;
	long start_pos;
	long end_pos;
};

struct f_meta{
	unsigned int termID;
	long start_pos;
	long end_pos;
};

struct mData{
	//need number of blocks?
	string term;
	int index_num;//in which static index is the postinglist stored
	int num_posting;//number of postings

	std::vector<fileinfo> file_info;//how a postinglist is stored in multiple files
	long meta_doc_start;
	long meta_frag_start;
	long meta_pos_start;
	long posting_start;
	long frag_start;
	long pos_start;
	//long ID_offset;
};

struct less_than_key
{
    inline bool operator() (const Posting& p1, const Posting& p2)
    {
        if(p1.termID == p2.termID){
        	if(p1.docID == p2.docID){
        		if(p1.fragID == p2.fragID){
        			return (p1.pos < p2.pos);
        		}else{
        			return (p1.fragID < p2.fragID);
        		}
        	}else{
        		return (p1.docID < p2.docID);
        	}
        }else{
        	return (p1.termID < p2.termID);
        }
    }
};

class Compressor{
public:
	std::vector<std::string> read_directory( std::string& path ){
		std::vector <std::string> result;
		dirent* de;
		DIR* dp;
		errno = 0;
		dp = opendir( path.empty() ? "." : path.c_str() );
  		if (dp){
    		while (true){
      			errno = 0;
      			de = readdir( dp );
      			if (de == NULL) break;
      			result.push_back( std::string( de->d_name ) );
      		}
			closedir( dp );
    	}
  		return result;
  	}

  	void update_f_meta(map<string, vector<f_meta>>& filemeta, string s1, string s2){
  		filemeta.erase(s1);
  		filemeta.erase(s2);
  	}

	void update_t_meta(unsigned int termID, string file1, string file2, map<unsigned int, mData>& dict){
		//delete old metadata, compress_p will take care of adding new
		vector<fileinfo>& infovec= dict[termID].file_info;
		for( vector<fileinfo>::iterator it = infovec.begin(); it != infovec.end(); it++){
			if( it->filename == file1 ) infovec.erase(it);
			else if( it->filename == file2 ) infovec.erase(it);
		}
	}

	void copy_and_paste(ifstream& ifile, ofstream& ofile, long start, long end){
		ifile.seekg(start);
		char c;
		while(ifile.tellg() != end){
			ifile.get(c);
			ofile << c;
		}
	}

	void merge(map<string, vector<f_meta>>& filemeta, int indexnum, map<unsigned int, mData>& dict){
		ifstream filez;
		ifstream filei;
		ofstream ofile;
		filez.open(string(PDIR) + "Z" + to_string(indexnum));
		filei.open(string(PDIR) + "I" + to_string(indexnum));

		ofile.open(string(PDIR) + "Z" + to_string(indexnum + 1), ios::ate | ios::binary);
		if(ofile.tellp() != 0){
			ofile.close();
			ofile.open(string(PDIR) + "I0", ios::ate | ios::binary);
		}

		long ostart;
		long oend;
		string file1 = "Z" + to_string(indexnum);
		string file2 = "I" + to_string(indexnum);
		vector<f_meta>& v1 = filemeta[file1];
		vector<f_meta>& v2 = filemeta[file2];
		vector<f_meta>::iterator it1 = v1.begin();
		vector<f_meta>::iterator it2 = v2.begin();

		//Go through the meta data of each file, do
		//if there is a termID appearing in both, decode the part and merge
		//else copy and paste the corresponding part of postinglist
		//update the corresponding fileinfo of that termID
		//assume that the posting of one term can be stored in memory
		while( it1 != v1.end() && it2 != v2.end() ){
			if( it1->termID == it2->termID ){
				//decode and merge
				//update meta data corresponding to the term
				vector<Posting> vp1 = Reader::decompress(file1, it1->termID, dict);
				vector<Posting> vp2 = Reader::decompress(file2, it2->termID, dict);
				vector<Posting> vpout; //store the sorted result
				//use NextGQ to write the sorted vector of Posting to disk
				vector<Posting>::iterator vpit1 = vp1.begin();
				vector<Posting>::iterator vpit2 = vp2.begin();
				while( vpit1 != vp1.end() && vpit2 != vp2.end() ){
					//NextGQ
					if( *vpit1 < *vpit2 ){
						vpout.push_back(*vpit1);
						vpit1 ++;
					}
					else if( *vpit1 > *vpit2 ){
						vpout.push_back(*vpit2);
						vpit2 ++;
					}
					else if ( *vpit1 == *vpit2 ){
						cout << "Error: same posting appearing in different indexes." << endl;
						break;
					}
				}

				update_t_meta(it1->termID, file1, file2, dict);
				compress_p(vpout, filemeta, dict, indexnum + 1);
				it1 ++;
				it2 ++;
			}
			else if( it1->termID < it2->termID ){
				copy_and_paste(filez, ofile, it1->start_pos, it1->end_pos);
				it1 ++;
			}
			else if( it1->termID > it2->termID ){
				copy_and_paste(filei, ofile, it2->start_pos, it2->end_pos);
				it2 ++;
			}
		}
		//following can be streamlined so that it is copied till end of file
		if (it1 != v1.end() ){
			for(vector<f_meta>::iterator it = it1; it != v1.end(); it++){
				copy_and_paste(filez, ofile, it->start_pos, it->end_pos);
			}
		}
		if (it2 != v2.end() ){
			for(vector<f_meta>::iterator it = it2; it != v2.end(); it++){
				copy_and_paste(filei, ofile, it2->start_pos, it2->end_pos);
			}
		}
		update_f_meta(filemeta, string(PDIR) + "Z" + to_string(indexnum), string(PDIR) + "I" + to_string(indexnum));
		filez.close();
		filei.close();
		ofile.close();
		//delete two old files
	}

	void merge_test(map<string, vector<f_meta>>& filemeta, map<unsigned int, mData>& dict){
		int indexnum = 0;
		vector<string> files = read_directory(string(PDIR));

		while(!none_of(begin(files), std::end(files), "I" + to_string(indexnum))){
			//if In exists already, merge In with Zn
			merge(filemeta, indexnum, dict);
			indexnum ++;
		}
	}

	void write(vector<uint8_t> num, ofstream& ofile){
		for(vector<uint8_t>::iterator it = num.begin(); it != num.end(); it++){
			ofile.write(reinterpret_cast<const char*>(&(*it)), 1);
		}
	}

	std::vector<char> read_com(ifstream& infile){
		char c;
		vector<char> result;
		while(infile.get(c)){
			result.push_back(c);
		}
		return result;
	}

	std::vector<uint8_t> VBEncode(unsigned int num){
		vector<uint8_t> result;
		uint8_t b;
		while(num >= 128){
			int a = num % 128;
			bitset<8> byte(a);
			byte.flip(7);
			num = (num - a) / 128;
			b = byte.to_ulong();
			result.push_back(b);
		}
		int a = num % 128;
		bitset<8> byte(a);
		b = byte.to_ulong();
		result.push_back(b);
		return result;
	}

	std::vector<uint8_t> VBEncode(vector<unsigned int>& nums){
		vector<uint8_t> biv;
		vector<uint8_t> result;
		for( vector<unsigned int>::iterator it = nums.begin(); it != nums.end(); it ++){
			biv = VBEncode(*it);
			result.insert(result.end(), biv.begin(), biv.end());
		}
		return result;
	}

	vector<uint8_t> compress(std::vector<unsigned int>& field, int method, int sort, vector<uint8_t> &meta_data_biv, vector<unsigned int> &last_id_biv){
		if(method){
			std::vector<unsigned int> block;
			std::vector<unsigned int>::iterator it = field.begin();
			std::vector<uint8_t> field_biv;
			std::vector<uint8_t> biv;

			unsigned int prev = *it;
			int size_block;
			while(it != field.end()){
				size_block = 0;
				block.clear();

				while(size_block < 64 && it != field.end()){
					block.push_back(*it - prev);
					prev = *it;
					size_block ++;
					it ++;
				}
				biv = VBEncode(block);
				last_id_biv.push_back(prev);//the first element of every block needs to be stored
				field_biv.insert(field_biv.end(), biv.begin(), biv.end());
				meta_data_biv.push_back(biv.size());//meta data stores the number of bytes after compression
			}
			return field_biv;
		}
	}

	vector<uint8_t> compress(std::vector<unsigned int>& field, int method, int sort, vector<uint8_t> &meta_data_biv){
		if(method){
			std::vector<unsigned int> block;
			std::vector<unsigned int>::iterator it = field.begin();
			std::vector<uint8_t> field_biv;
			std::vector<uint8_t> biv;


			int prev;
			int size_block;
			while(it != field.end()){
				size_block = 0;
				block.clear();
				prev = 0;//the first element of every block needs to be renumbered

				while(size_block < 64 && it != field.end()){
					block.push_back(*it - prev);
					prev = *it;
					size_block ++;
					it ++;
				}
				biv = VBEncode(block);

				field_biv.insert(field_biv.end(), biv.begin(), biv.end());
				meta_data_biv.push_back(biv.size());//meta data stores the number of bytes after compression
			}
			return field_biv;
		}
	}

	mData compress_p(string filename, ofstream& ofile, f_meta fm, std::vector<unsigned int>& v_docID, std::vector<unsigned int>& v_fragID, std::vector<unsigned int>& v_pos){

		std::vector<unsigned int> v_last_id;
		std::vector<uint8_t> docID_biv;
		std::vector<uint8_t> fragID_biv;
		std::vector<uint8_t> pos_biv;

		std::vector<uint8_t> last_id_biv;
		std::vector<uint8_t> size_doc_biv;
		std::vector<uint8_t> size_frag_biv;
		std::vector<uint8_t> size_pos_biv;

		docID_biv = compress(v_docID, 1, 1, size_doc_biv, v_last_id);
		last_id_biv = VBEncode(v_last_id);

		fragID_biv = compress(v_fragID, 1, 0, size_frag_biv);
		pos_biv = compress(v_pos, 1, 0, size_pos_biv);

		mData meta;
		fileinfo fi;
		fi.filename = filename;
		fi.start_pos = ofile.tellp();
		fm.start_pos = ofile.tellp();
		write(last_id_biv, ofile);

		meta.meta_doc_start = ofile.tellp();
		write(size_doc_biv, ofile);

		meta.meta_frag_start = ofile.tellp();
		write(size_frag_biv, ofile);

		meta.meta_pos_start = ofile.tellp();
		write(size_pos_biv, ofile);

		meta.posting_start = ofile.tellp();
		write(docID_biv, ofile);

		meta.frag_start = ofile.tellp();
		write(fragID_biv, ofile);

		meta.pos_start = ofile.tellp();
		write(pos_biv, ofile);

		fi.end_pos = ofile.tellp();
		meta.file_info.push_back(fi);//store the start and end position of postinglist in this file
		fm.end_pos = ofile.tellp();

		return meta;
	}

	void compress_p(std::vector<Posting>& pList, std::map<string, vector<f_meta>>& filemeta, map<unsigned int, mData>& dict, int indexnum = 0){
		//pass in forward index of same termID
		//compress positional index
		ofstream ofile;//positional inverted index
		string filename = string(PDIR) + "Z" + to_string(indexnum);
		ofile.open(filename, ios::ate | ios::binary);
		if(ofile.tellp() != 0){
			ofile.close();
			filename = string(PDIR) + "I" + to_string(indexnum);
		}
		ofile.open(filename, ios::ate | ios::binary);

		std::vector<unsigned int> v_docID;
		std::vector<unsigned int> v_fragID;
		std::vector<unsigned int> v_pos;
		mData mmData;
		f_meta fm;
		unsigned int num_of_p = 0;//number of posting of a certain term

		unsigned int currID = pList[0].termID;//the ID of the term that is currently processing
		for(std::vector<Posting>::iterator it = pList.begin(); it != pList.end(); it++){
			while(it->termID == currID){
				v_docID.push_back(it->docID);
				v_fragID.push_back(it->fragID);
				v_pos.push_back(it->pos);
				it ++;
				num_of_p ++;
			}
			fm.termID = currID;
			currID = it->termID;
			mmData = compress_p(filename, ofile, fm, v_docID, v_fragID, v_pos);
			mmData.num_posting = num_of_p;
			filemeta[filename].push_back(fm);

			//add mmdata to the dictionary of corresponding term
			dict[currID] = mmData;

			num_of_p = 0;
			v_docID.clear();
			v_fragID.clear();
			v_pos.clear();
			it --;//before exit while loop, iterator is added but the corresponding value is not pushed to vector
		}

		ofile.close();
		merge_test(filemeta, dict);//see if need to merge
	}

	void start_compress(map<string, vector<f_meta>>& filemeta, map<unsigned int, mData>& dict){
		vector<Posting> invert_index;
		ifstream index;
		ifstream info;
		index.open(INDEX);
		info.open(INFO);

		string line;
		string value;
		vector<string> vec;
		char c;
		int p;
		int num;

		for(int i = 0; i < NO_DOC; i ++){
			vec.clear();
			getline(info, line);//read docInfo
			stringstream lineStream(line);
			unsigned int pos = 0;

			while(lineStream >> value){
				vec.push_back(value);
			}

			index.seekg(stoi(vec[2]));
			while(index.tellg() != (stoi(vec[2]) + stoi(vec[3]))){
				//for every document, do
				index.get(c);
				bitset<8> byte(c);
				num = 0; // store decoding termID
				p = 0;//power
				while(byte[7] == 1){
					byte.flip(7);
					num += byte.to_ulong()*pow(128, p);
					p++;
					index.get(c);
					byte = bitset<8>(c);
				}
				num += (byte.to_ulong())*pow(128, p);
				pos ++;

				Posting p(num, stoul(vec[1]), 0, pos);
				invert_index.push_back(p);
				if (invert_index.size() > POSTING_LIMIT){ // make sure doesn't exceed memory
					std::sort(invert_index.begin(), invert_index.end(), less_than_key());
					compress_p(invert_index, filemeta, dict);
					invert_index.clear();
				}
			}

		}
		index.close();
		info.close();
	}
};

std::vector<char> Reader::read_com(ifstream& infile){//read compressed forward index
	char c;
	vector<char> result;
	while(infile.get(c)){
		result.push_back(c);
	}
	return result;
}

std::vector<unsigned int> Reader::VBDecode(ifstream& ifile, long start_pos = 0, long end_pos = ifile.tellg()){//ios::ate
	ifile.seekg(start_pos);
	char c;
	unsigned int num;
	int p;
	vector<unsigned int> result;
	vector<char> vec = read_com(ifile);

	for(vector<char>::iterator it = vec.begin(); it != vec.end(); it++){
		c = *it;
		bitset<8> byte(c);
		num = 0;
		p = 0;
		while(byte[7] == 1){
			byte.flip(7);
			num += byte.to_ulong()*pow(128, p);
			p++;
			it ++;
			c = *it;
			byte = bitset<8>(c);
		}
		num += (byte.to_ulong())*pow(128, p);

		result.push_back(num);
	}
	return result;
}

std::vector<unsigned int> Reader::VBDecode(vector<char>& vec){
	unsigned int num;
	vector<unsigned int> result;
	char c;
	int p;
	for(vector<char>::iterator it = vec.begin(); it != vec.end(); it++){
		c = *it;
		bitset<8> byte(c);
		num = 0;
		p = 0;
		while(byte[7] == 1){
			byte.flip(7);
			num += byte.to_ulong()*pow(128, p);
			p++;
			it ++;
			c = *it;
			byte = bitset<8>(c);
		}
		num += (byte.to_ulong())*pow(128, p);

		result.push_back(num);
	}
	return result;
}

std::vector<Posting> Reader::decompress(string filename, unsigned int termID, map<unsigned int, mData>& dict){
	ifstream ifile;
	ifile.open(filename, ios::binary);
	vector<Posting> result;
	Posting p;
	char c;
	vector<char> readin;

	vector<unsigned int> docID;
	vector<unsigned int> fragID;
	vector<unsigned int> pos;

	ifile.seekg(dict[termID].posting_start);
	while(ifile.tellg != dict[termID].frag_start){
		ifile.get(c);
		readin.push_back(c);
		docID = VBDecode(readin);
		readin.clear();
	}

	ifile.seekg(dict[termID].frag_start);
	while(ifile.tellg != dict[termID].pos_start){
		ifile.get(c);
		readin.push_back(c);
		fragID = VBDecode(readin);
		readin.clear();
	}

	ifile.seekg(dict[termID].pos_start);
	while(ifile.tellg != dict[termID].file_info.end_pos){
		ifile.get(c);
		readin.push_back(c);
		pos = VBDecode(readin);
		readin.clear();
	}

	vector<unsigned int>::iterator itdoc = docID.begin();
	vector<unsigned int>::iterator itfrag = fragID.begin();
	vector<unsigned int>::iterator itpos = pos.begin();

	while(itdoc != docID.end()){
		result.push_back(p(termID, *itdoc, *itfrag, *itpos));
		itdoc ++;
		itfrag ++;
		itpos ++;
	}

	return result;
}

int main(){
	Compressor comp;
	map<unsigned int, mData> dict;
	map<string, vector<f_meta>> filemeta;
	comp.start_compress(filemeta, dict);

	return 0;
}
