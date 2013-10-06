/*
 *  FinalWordRhymeModel.cpp
 *
 *  Copyright 2013 by Joerg Tiedemann. All rights reserved.
 *
 *  This file is part of Docent, a document-level decoder for phrase-based
 *  statistical machine translation.
 *
 *  Docent is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software
 *  Foundation, either version 3 of the License, or (at your option) any later
 *  version.
 *
 *  Docent is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  Docent. If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <fstream>
#include <cstdlib>

using namespace std;

#include "Docent.h"
#include "DocumentState.h"
#include "FeatureFunction.h"
#include "SearchStep.h"
#include "FinalWordRhymeModel.h"

#include <boost/unordered_map.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/regex.hpp>

struct FinalWordRhymeModelState : public FeatureFunction::State, public FeatureFunction::StateModifications {
  FinalWordRhymeModelState(uint nsents, uint maxGap) : 
    wordList(nsents), rhymeMap(nsents), rhymeEntropy(nsents), currentScore(0) {
    docSize = nsents; 
    maxGapSize = maxGap;
    wordList.resize(docSize);
  }

  uint docSize;
  uint maxGapSize;

  typedef boost::unordered_map<std::string,boost::dynamic_bitset<> > WordMap_;
  typedef boost::unordered_map<std::string,float> WordEntropy_;
  typedef boost::unordered_map<uint,uint> GapFreq_;

  std::vector<std::string> wordList;
  WordMap_ rhymeMap;
  WordEntropy_ rhymeEntropy;
  float currentScore;

  Float score() const {
    return -Float(1/currentScore);
  }



  /*
  void updateScore(const uint &sentno) {
    for (WordMap_::const_iterator r_it = rhymeMap.begin(); r_it != rhymeMap.end(); ++r_it ) {
      if (r_it->second.test(sentno)){
	WordEntropy_::const_iterator e_it;
	e_it = rhymeEntropy.find(r_it->first);
	if (e_it != rhymeEntropy.end()){


      }
    }
  */


  // TODO: only count one alternative pronounciation!
  // now: words with several patterns contribute more than others!
  // idea: add highest rhyme entropy value for each final word?

  void updateScore() {

    boost::dynamic_bitset<> covered(docSize);

    float oldScore = currentScore;
    currentScore = 0;
    covered.reset();

    for (WordEntropy_::const_iterator it = rhymeEntropy.begin(); it != rhymeEntropy.end(); ++it ) {
      currentScore+=it->second;
      WordMap_::const_iterator r_it;
      r_it = rhymeMap.find(it->first);
      if (r_it != rhymeMap.end()){
	// XOR of bitsets
	boost::dynamic_bitset<> combined((covered^=r_it->second));
	covered = combined;
	/*
	cerr << " ---- "  << combined << " = " << ratio << " = " << currentScore << endl;
	*/
      }
    }

    // how many lines are covered?
    Float ratio = (float) covered.count() / docSize;
    currentScore *= ratio;
    

    if (currentScore > oldScore){
      cerr << "-----------------------------" << endl;
      // cerr << oldScore << " -> " << currentScore << endl;
      cerr << -Float(1/oldScore) << " -> " << -Float(1/currentScore) << endl;
      cerr << "-----------------------------" << endl;
      printRhymeMap();
      printRhymeEntropies();
    }

  }


  void computeAllEntropies() {
    computeAllEntropies(rhymeMap,rhymeEntropy);
  }

  void computeAllEntropies(const WordMap_ &map, WordEntropy_ &entropy) {
    for (WordMap_::const_iterator it = map.begin(); it != map.end(); ++it ) {
      computeEntropy(map,it->first,entropy);
    }
  }


  void printRhymeMap() const {
    for (WordMap_::const_iterator it = rhymeMap.begin(); it != rhymeMap.end(); ++it ) {
      if (it->second.count() > 1){
	cerr << it->second << it->first << endl;
      }
    }
  }

  void printRhymeEntropies() const {
    for (WordEntropy_::const_iterator it = rhymeEntropy.begin(); it != rhymeEntropy.end(); ++it ) {
      if (it->second > 0){
	cerr << "entropy: " << it->first << " = " << it->second << endl;
      }
    }
  }


  void computeEntropy(const WordMap_ &map, const std::string word, WordEntropy_ &entropy){
    WordMap_::const_iterator it;
    it = map.find(word);
    if (it == map.end()){
      return;
    }

    GapFreq_ gapFreq;

    /// TODO: could use bitset.find_next instead of for-loop (more efficient?)

    uint countWordChange = 0;
    uint countWordPairs = 0;
    std::string &lastWord = wordList[0];

    uint lastPos = 0;
    // boost::dynamic_bitset<> bitmap(docSize);
    // bitmap = (it->second);
    const boost::dynamic_bitset<> &bitmap(it->second);
    // for(uint i = 0; i < bitmap.size(); ++i){
    for(uint i = bitmap.find_first(); i < bitmap.size(); ++i){
      if (bitmap.test(i)){
	if (lastPos > 0){
	  uint gap = i+1-lastPos;
	  if (gap <= maxGapSize){
	    gapFreq[gap]++;
	    countWordPairs++;
	    if (lastWord != wordList[i]){
	      // cerr << "rhyme " << word << " changed word from " << lastWord << " to " << wordList[i] << endl;
	      // cerr << "last pos = " << lastPos-1 << " new pos " << i << endl;
	      // cerr << wordList << endl;
	      countWordChange++;
	    }
	  }
	}
	lastWord=wordList[i];
	lastPos=i+1;
      }
    }

    // uint nrGaps = gapFreq.size();
    uint nrGaps = bitmap.count()-1;
    entropy[word] = 0;
    for (GapFreq_::const_iterator g_it = gapFreq.begin(); g_it != gapFreq.end(); ++g_it ) {
      float prob = float(g_it->second) / float(nrGaps);
      entropy[word] -= prob * log(prob) / log(2);
    }

    if (countWordPairs > 0){
      // cerr << " score: " << entropy[word] << " (" << countWordChange << "/" << countWordPairs;
      entropy[word] *= float(countWordChange+1) / float(countWordPairs+1);
      // cerr << ") - " << entropy[word] << endl;
    }

  }


  void setWord(const uint &sentno, const std::string &word, const std::string &rhyme){
    // TODO: really skip all cases where word = rhyme?
    if (rhyme == word){ return; }
    WordMap_::const_iterator it = rhymeMap.find(rhyme);
    if (it == rhymeMap.end()){
      rhymeMap[rhyme].resize(docSize);
    }
    rhymeMap[rhyme].set(sentno);
    wordList[sentno] = word;
    //    cerr << sentno << " ... rhyme ... " << rhyme << " word " << word << endl; 
  }



  void resetWord(const uint &sentno, const std::string &word, const std::string &rhyme){
    // TODO: really skip all cases where word = rhyme?
    if (rhyme == word){ return; }
    WordMap_::const_iterator it = rhymeMap.find(rhyme);
    if (it == rhymeMap.end()){
      /*
      cerr << " ... not found? " << sentno << " " << word << " - " << rhyme << endl;
      printRhymeMap();
      printRhymeEntropies();
      */
      rhymeMap[rhyme].resize(docSize);
      // exit(1);
    }
    rhymeMap[rhyme].reset(sentno);
  }



  void changeWord(const uint &sentno, 
		  const std::string &oldWord, const std::vector<std::string> &oldRhyme,
		  const std::string &newWord, const std::vector<std::string> &newRhyme){

    bool needUpdate=false;
    for (uint i=0;i<oldRhyme.size();++i){
      if(std::find(newRhyme.begin(), newRhyme.end(), oldRhyme[i])!=newRhyme.end()){
	resetWord(sentno,oldWord,oldRhyme[i]);
	computeEntropy(rhymeMap,oldRhyme[i],rhymeEntropy);
	needUpdate=true;
      }
    }
    for (uint i=0;i<newRhyme.size();++i){
      if(std::find(oldRhyme.begin(), oldRhyme.end(), newRhyme[i])!=oldRhyme.end()){
	setWord(sentno,newWord,newRhyme[i]);
	computeEntropy(rhymeMap,newRhyme[i],rhymeEntropy);
	needUpdate=true;
      }
    }
    if (needUpdate){
      updateScore();
    }

    /*
    if (! equal(oldRhyme.begin(), oldRhyme.end(), newRhyme.begin()) ){
      updateScore();
    }
    */

    /*
    if (oldRhyme != newRhyme){
      resetWord(sentno,oldWord,oldRhyme);
      setWord(sentno,newWord,newRhyme);
      computeEntropy(rhymeMap,oldRhyme,rhymeEntropy);
      computeEntropy(rhymeMap,newRhyme,rhymeEntropy);
      updateScore();
    }
    */
  }


  virtual FinalWordRhymeModelState *clone() const {
    return new FinalWordRhymeModelState(*this);
  }
};





void FinalWordRhymeModel::initializeRhymeModel(const Parameters &params){
  maxRhymeDistance = params.get<int>("max-rhyme-distance", 4);
  std::string file = params.get<std::string>("rhymes-file", "");
  if (file != ""){
    ifstream infile(file.c_str());
    std::string word, rhyme;
    while ( infile >> word >> rhyme ){
      rhymes[word].push_back(rhyme);
    }
    infile.close();
    for(Rhymes_::const_iterator it = rhymes.begin(); it != rhymes.end(); ++it) {
      rhymes[it->first].push_back(getLastSyllable(it->first));
    }

  }

  // lastSyllRE: simple regex-heuristics to extract final rhyme syllables
  // TODO: move regexes to config files
  // --> make it more flexible for other languages
  lastSyllRE.push_back(boost::regex (".*[^aeiou]([aeiou].*?e[nr])"));
  lastSyllRE.push_back(boost::regex (".*[^aeiou]([aeiou]..*?)"));
  lastSyllRE.push_back(boost::regex ("^([aeiou].*)"));
}

void FinalWordRhymeModel::printPronounciationTable() const {
  typedef boost::unordered_map<std::string,vector<std::string> > Rhymes_;
  cerr << "-----------------------------------" << endl;
  for (Rhymes_::const_iterator it = rhymes.begin(); it != rhymes.end(); ++it ) {
    cerr << " *** " << it->first << " = " << it->second << endl;
  }
  cerr << "-----------------------------------" << endl;
}


FeatureFunction::State *FinalWordRhymeModel::initDocument(const DocumentState &doc, Scores::iterator sbegin) const {
	const std::vector<PhraseSegmentation> &sentences = doc.getPhraseSegmentations();

	FinalWordRhymeModelState *s = new FinalWordRhymeModelState(sentences.size(),maxRhymeDistance);

	for(uint i = 0; i < sentences.size(); i++){
	  const std::string &word = *sentences[i].rbegin()->second.get().getTargetPhrase().get().rbegin();
	  const std::vector<std::string> rhyme = getRhyme(word);
	  // cerr << " + " << i << " " << word << " - " << rhyme;
	  if (rhyme.size()>0){
	    for (uint j=0;j<rhyme.size();++j){
	      // cerr << ".";
	      s->setWord(i,word,rhyme[j]);
	    }
	    // cerr << " ok " << endl;
	  }
	}
	s->computeAllEntropies();
	s->updateScore();

	/*
	cerr << "+++++++++++++++++++++++++++++++++" << endl;
	s->printRhymeMap();
	s->printRhymeEntropies();
	cerr << "+++++++++++++++++++++++++++++++++" << endl;
	*/

	*sbegin = s->score();
	return s;
}


void FinalWordRhymeModel::computeSentenceScores(const DocumentState &doc, uint sentno, Scores::iterator sbegin) const {
	*sbegin = Float(0);
}

const std::vector<std::string> FinalWordRhymeModel::getRhyme(const std::string &word) const{
  boost::unordered_map<std::string,vector<std::string> >::const_iterator it;

  it = rhymes.find(word);
  if (it != rhymes.end()){
    return it->second;
  }

  std::vector<std::string> syll(1);
  syll[0] = getLastSyllable(word);
  return syll;
}

const std::string FinalWordRhymeModel::getLastSyllable(const std::string &word) const{

  // matches[0] contains the original string.
  // matches[n] contains a sub_match object for each matching subexpression
  // sub_match::first and sub_match::second are iterators that
  // refer to the first and the last chars of the matching subexpression

  if (lastSyllRE.size()>0){
    boost::cmatch matches; 
    for (uint i = 0; i<lastSyllRE.size();++i){
      if (boost::regex_match(word.c_str(), matches, lastSyllRE[i])){
	// cerr << " found matches: " << matches.size() << " - " << matches[1].first << " + " 
	//      << matches[1].second << endl;
	return matches[1].first;
      }
    }
  }

  else{

    // fallback for English if no lastSyllRE is given
    boost::cmatch matches; 
    boost::regex re0(".*[^aeiou]([aeiou].*?e[nr])");
    if (boost::regex_match(word.c_str(), matches, re0)){
      return matches[1].first;
    }
    boost::regex re1(".*[^aeiou]([aeiou]..*?)");
    if (boost::regex_match(word.c_str(), matches, re1)){
      return matches[1].first;
    }
    boost::regex re2("^([aeiou].*)");
    if (boost::regex_match(word.c_str(), matches, re2)){
      return matches[1].first;
    }
  }
  return word;
}


FeatureFunction::StateModifications *FinalWordRhymeModel::estimateScoreUpdate(const DocumentState &doc, const SearchStep &step, const State *state,
		Scores::const_iterator psbegin, Scores::iterator sbegin) const {

	const FinalWordRhymeModelState *prevstate = dynamic_cast<const FinalWordRhymeModelState *>(state);
	FinalWordRhymeModelState *s = prevstate->clone();
  
	const std::vector<SearchStep::Modification> &mods = step.getModifications();
	for(std::vector<SearchStep::Modification>::const_iterator it = mods.begin(); it != mods.end(); ++it) {
	  uint sentno = it->sentno;
	  const PhraseSegmentation &sentence = doc.getPhraseSegmentation(sentno);

	  if (it->to_it == sentence.end()) {

	    const std::string &oldWord = *sentence.rbegin()->second.get().getTargetPhrase().get().rbegin();
	    const std::vector<std::string> oldRhyme = getRhyme(oldWord);

	    const std::string &newWord = *it->proposal.rbegin()->second.get().getTargetPhrase().get().rbegin();
	    const std::vector<std::string> newRhyme = getRhyme(newWord);

	    s->changeWord(sentno,oldWord,oldRhyme,newWord,newRhyme);

	  }
	}
	*sbegin = s->score();
	return s;
}


FeatureFunction::StateModifications *FinalWordRhymeModel::updateScore(const DocumentState &doc, const SearchStep &step, const State *state,
		FeatureFunction::StateModifications *estmods, Scores::const_iterator psbegin, Scores::iterator estbegin) const {
  return estmods;
}


FeatureFunction::State *FinalWordRhymeModel::applyStateModifications(FeatureFunction::State *oldState, FeatureFunction::StateModifications *modif) const {
	FinalWordRhymeModelState *os = dynamic_cast<FinalWordRhymeModelState *>(oldState);
	FinalWordRhymeModelState *ms = dynamic_cast<FinalWordRhymeModelState *>(modif);
	os->rhymeMap.swap(ms->rhymeMap);
	os->rhymeEntropy.swap(ms->rhymeEntropy);
	os->wordList.swap(ms->wordList);
	os->currentScore = ms->currentScore;
	return os;
}
