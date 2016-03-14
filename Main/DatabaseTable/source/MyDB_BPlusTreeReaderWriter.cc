
#ifndef BPLUS_C
#define BPLUS_C

#include <algorithm>
#include <string.h>
#include "MyDB_INRecord.h"
#include "MyDB_BPlusTreeReaderWriter.h"
#include "MyDB_PageReaderWriter.h"
#include "MyDB_PageListIteratorSelfSortingAlt.h"
#include "RecordComparator.h"

MyDB_BPlusTreeReaderWriter :: MyDB_BPlusTreeReaderWriter (string orderOnAttName, MyDB_TablePtr forMe, 
	MyDB_BufferManagerPtr myBuffer) : MyDB_TableReaderWriter (forMe, myBuffer) {

	// find the ordering attribute
	auto res = forMe->getSchema ()->getAttByName (orderOnAttName);

	// remember information about the ordering attribute
	orderingAttType = res.second;
	whichAttIsOrdering = res.first;
}

MyDB_RecordIteratorAltPtr MyDB_BPlusTreeReaderWriter :: getSortedRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high) {

	// this is the list of pages that we need to iterate over
	vector <MyDB_PageReaderWriter> list;

	// fill up this list of pages
	discoverPages (rootLocation, list, low, high);

	// for various comparisons
	MyDB_RecordPtr lhs = getEmptyRecord ();
	MyDB_RecordPtr rhs = getEmptyRecord ();
	MyDB_RecordPtr myRec = getEmptyRecord ();
	MyDB_INRecordPtr llow = getINRecord ();
	llow->setKey (low);
	MyDB_INRecordPtr hhigh = getINRecord ();
	hhigh->setKey (high);

	// build the comparison functions
	function <bool ()> comparator = buildComparator (lhs, rhs);
	function <bool ()> lowComparator = buildComparator (myRec, llow);	
	function <bool ()> highComparator = buildComparator (hhigh, myRec);	

	// and build the iterator
	return make_shared <MyDB_PageListIteratorSelfSortingAlt> (list, lhs, rhs, comparator, myRec, lowComparator, highComparator, true);	
}

MyDB_RecordIteratorAltPtr MyDB_BPlusTreeReaderWriter :: getRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high) {

	// this is the list of pages that we need to iterate over
	vector <MyDB_PageReaderWriter> list;

	// fill up this list of pages
	discoverPages (rootLocation, list, low, high);

	// for various comparisons
	MyDB_RecordPtr lhs = getEmptyRecord ();
	MyDB_RecordPtr rhs = getEmptyRecord ();
	MyDB_RecordPtr myRec = getEmptyRecord ();
	MyDB_INRecordPtr llow = getINRecord ();
	llow->setKey (low);
	MyDB_INRecordPtr hhigh = getINRecord ();
	hhigh->setKey (high);

	// build the comparison functions
	function <bool ()> comparator = buildComparator (lhs, rhs);
	function <bool ()> lowComparator = buildComparator (myRec, llow);	
	function <bool ()> highComparator = buildComparator (hhigh, myRec);	

	// and build the iterator
	return make_shared <MyDB_PageListIteratorSelfSortingAlt> (list, lhs, rhs, comparator, myRec, lowComparator, highComparator, false);	
}


bool MyDB_BPlusTreeReaderWriter :: discoverPages (int whichPage, vector <MyDB_PageReaderWriter> &list,
	MyDB_AttValPtr low, MyDB_AttValPtr high) {
    
	// figure out the page to search
	MyDB_PageReaderWriter pageToSearch = (*this)[whichPage];

	// it is a regular page (data page)
	if (pageToSearch.getType () == MyDB_PageType :: RegularPage) {

		/* Your code here */
        // add the page to the list
		list.push_back (pageToSearch);

		return true;
		
	// we have an internal node, so find the subtrees to seach
	} else {

		/* You might want to change this, or just go with what I have!! */

		// iterate through the various subtrees
		MyDB_RecordIteratorAltPtr temp = pageToSearch.getIteratorAlt ();

		// set up all of the comparisons that we need
		MyDB_INRecordPtr otherRec = getINRecord ();
		MyDB_INRecordPtr llow = getINRecord ();
		llow->setKey (low);
		MyDB_INRecordPtr hhigh = getINRecord ();
		hhigh->setKey (high);
		function <bool ()> comparatorLow = buildComparator (otherRec, llow);
		function <bool ()> comparatorHigh = buildComparator (hhigh, otherRec);

		/* Your code here */
		int lowPage = -1;
        vector <int> pageList;

        // find all possible page entries
		while (temp->advance ())
		{
			temp->getCurrent (otherRec);
			if (lowPage == -1 && ! comparatorLow ())
                lowPage = otherRec->getPtr ();
			if (lowPage != -1)
            {
                pageList.push_back(otherRec->getPtr ());
                if (comparatorHigh ())
                    break;
            }
		}
        
        // discover pages recursively
        for (vector<int>::iterator it = pageList.begin() ; it != pageList.end(); ++it)
        {
            discoverPages (*it, list, low, high);
        }

		return false;

	}

	// note, we should never get here
	return false;
}

void MyDB_BPlusTreeReaderWriter :: append (MyDB_RecordPtr appendMe) {

	// this file has never had any data in it, because the smallest B+-Tree has two pages
	if (getNumPages () <= 1) {
		
		// the root is at page location zero
		MyDB_PageReaderWriter root = (*this)[0];
		rootLocation = 0;

		// get an internal node record that has a pointer to page 1
		MyDB_INRecordPtr internalNodeRec = getINRecord ();	
		internalNodeRec->setPtr (1);
		getTable ()->setLastPage (1);

		// add that internal node record in
		root.clear ();
		root.append (internalNodeRec);
		root.setType (MyDB_PageType :: DirectoryPage);
		
		// and add the new record to the leaf
		MyDB_PageReaderWriter leaf = (*this)[1];
		leaf.clear ();
		leaf.setType (MyDB_PageType :: RegularPage);
		leaf.append (appendMe);

	// this is a valid B+-Tree, so we can process the insert
	} else {

		// append the record into the tree
		auto res = append (rootLocation, appendMe);
		
		// see if the root split
		if (res != nullptr) {

			// add another page to the file
			int newRootLoc = getTable ()->lastPage () + 1;
			getTable ()->setLastPage (newRootLoc);
			MyDB_PageReaderWriter newRoot = (*this)[newRootLoc];
			newRoot.clear ();
			newRoot.setType (MyDB_PageType :: DirectoryPage);

			// add the two records; the first points to the newly-created page, the second to the old root
			newRoot.append (res);
			MyDB_INRecordPtr newRec = getINRecord ();
			newRec->setPtr (rootLocation);
			newRoot.append (newRec);

			// and update the location of the root
			rootLocation = newRootLoc;
		}
	}
}

#define NUM_BYTES_USED *((size_t *) (((char *) temp) + sizeof (size_t)))

MyDB_RecordPtr MyDB_BPlusTreeReaderWriter :: split (MyDB_PageReaderWriter splitMe, MyDB_RecordPtr andMe) {
	
	// get a new page for the lower one half
	int newPageLoc = getTable ()->lastPage () + 1;
	getTable ()->setLastPage (newPageLoc);
	MyDB_PageReaderWriter newPage = (*this)[newPageLoc];

	// remember the type of this page so we can re-create it after the clear
	MyDB_PageType myType;

	// get a couple of records and a comparator so that we can sort
	MyDB_RecordPtr lhs, rhs;
	if (splitMe.getType () == MyDB_PageType :: RegularPage) {
		lhs = getEmptyRecord ();
		rhs = getEmptyRecord ();
		myType = MyDB_PageType :: RegularPage;
	} else if (splitMe.getType () == MyDB_PageType :: DirectoryPage) {
		lhs = getINRecord ();
		rhs = getINRecord ();
		myType = MyDB_PageType :: DirectoryPage;
	}
	function <bool ()> comparator = buildComparator (lhs, rhs);	

	// temp memory to hold all of the records
	void *temp = malloc (splitMe.getPageSize ());
	memcpy (temp, splitMe.getBytes (), splitMe.getPageSize ());

	// positions of the records
	vector <void *> positions;

	// compute where all of the records are located
	int bytesConsumed = sizeof (size_t) * 2;
	while (bytesConsumed != NUM_BYTES_USED) {
		void *pos = bytesConsumed + (char *) temp;
		positions.push_back (pos);
		void *nextPos = lhs->fromBinary (pos);
		bytesConsumed += ((char *) nextPos) - ((char *) pos);
	}
	
	// and get a postition for the last guy
	void *spaceForLastGuy = malloc (andMe->getBinarySize ());
	andMe->toBinary (spaceForLastGuy);
	positions.push_back (spaceForLastGuy);

	// now sort
	RecordComparator myComparator (comparator, lhs, rhs);
	std::sort (positions.begin (), positions.end (), myComparator);

	// get the record to return
	MyDB_INRecordPtr returnVal = getINRecord ();
	returnVal->setPtr (newPageLoc);

	// clear the pages
	newPage.clear ();
	splitMe.clear ();
	newPage.setType (myType);
	splitMe.setType (myType);

	// and copy the data over
	int counter = 0;
	for (void *pos : positions) {

		// low data goes into the new page
		lhs->fromBinary (pos);
		if (counter < positions.size () / 2) 
			newPage.append (lhs);

		// median goes into the old page
		if (counter == positions.size () / 2) {
			newPage.append (lhs);
			returnVal->setKey (getKey (lhs));
		}

		// high data goes into the old page
		if (counter > positions.size () / 2)
			splitMe.append (lhs);

		counter++;
	}

	free (spaceForLastGuy);
	free (temp);

	return returnVal;

}

MyDB_RecordPtr MyDB_BPlusTreeReaderWriter :: append (int whichPage, MyDB_RecordPtr appendMe) {

	// figure out the page to add to
	MyDB_PageReaderWriter pageToAddTo = (*this)[whichPage];

	// it is a regular page (data page)
	if (pageToAddTo.getType () == MyDB_PageType :: RegularPage) {

        /* your code here! */

		// append the record to the data page
		if (pageToAddTo.append (appendMe))
		{
			return nullptr;
        }

		// split the page if the page is full
		return split (pageToAddTo, appendMe);

	// we have an internal node, so find the subtree to insert into
	} else {

		/* you can change this, or just use it */

		// iterate through the various subtrees
		MyDB_RecordIteratorAltPtr temp = pageToAddTo.getIteratorAlt ();
		MyDB_INRecordPtr otherRec = getINRecord ();
		function <bool ()> comparator = buildComparator (otherRec, appendMe);

		/* your code here! */
		int nextPage;

        // find the page entry
		while (temp->advance ())
		{
			temp->getCurrent (otherRec);
			if (!comparator ())
            {
                nextPage = otherRec->getPtr ();
                break;
            }
		}

        // insert the record from the page entry
		auto res = append (nextPage, appendMe);

        // returns if there is no page split
		if (res == nullptr)
		{
			return nullptr;
		}

		// insert the node of the new page to the current page and sort the current page
		if (pageToAddTo.append (res))
		{
			MyDB_INRecordPtr lhs = getINRecord();
			MyDB_INRecordPtr rhs = getINRecord();
			function <bool ()> sortComparator = buildSortComparator (lhs, rhs);
			pageToAddTo.sortInPlace (sortComparator, lhs, rhs);

			return nullptr;
		}

		return split (pageToAddTo, res);
	}

	// note, we should never get here
	return nullptr;
}

MyDB_INRecordPtr MyDB_BPlusTreeReaderWriter :: getINRecord () {
	return make_shared <MyDB_INRecord> (orderingAttType->createAttMax ());
}

void MyDB_BPlusTreeReaderWriter :: printTree () {
	printTree (rootLocation, 0);
}

void MyDB_BPlusTreeReaderWriter :: printTree (int whichPage, int depth) {

	MyDB_PageReaderWriter pageToPrint = (*this)[whichPage];

	// print out a leaf page
	if (pageToPrint.getType () == MyDB_PageType :: RegularPage) {
		MyDB_RecordPtr myRec = getEmptyRecord ();
		MyDB_RecordIteratorAltPtr temp = pageToPrint.getIteratorAlt ();
		while (temp->advance ()) {
			
			temp->getCurrent (myRec);
			for (int i = 0; i < depth; i++)
				cout << "\t";
			cout << myRec << "\n";
		}

	// print out a directory page
	} else {

		MyDB_INRecordPtr myRec = getINRecord ();
		MyDB_RecordIteratorAltPtr temp = pageToPrint.getIteratorAlt ();
		while (temp->advance ()) {
			
			temp->getCurrent (myRec);
			printTree (myRec->getPtr (), depth + 1);
			for (int i = 0; i < depth; i++)
				cout << "\t";
			cout << (MyDB_RecordPtr) myRec << "\n";
		}
	}
}

MyDB_AttValPtr MyDB_BPlusTreeReaderWriter :: getKey (MyDB_RecordPtr fromMe) {

	MyDB_AttValPtr source;

	// in this case, got an IN record
	if (fromMe->getSchema () == nullptr) 
		source = fromMe->getAtt (0);

	// in this case, got a data record
	else 
		source = fromMe->getAtt (whichAttIsOrdering);

	// get a copy of him
	void *space = malloc (source->getBinarySize ());
	source->toBinary (space);
	MyDB_AttValPtr returnVal = orderingAttType->createAtt ();
	returnVal->fromBinary (space);
	return returnVal;
	
}

function <bool ()>  MyDB_BPlusTreeReaderWriter :: buildComparator (MyDB_RecordPtr lhs, MyDB_RecordPtr rhs) {
    
    MyDB_AttValPtr lhAtt, rhAtt;
    
    // in this case, the LHS is an IN record
    if (lhs->getSchema () == nullptr) {
        lhAtt = lhs->getAtt (0);
        
        // here, it is a regular data record
    } else {
        lhAtt = lhs->getAtt (whichAttIsOrdering);
    }
    
    // in this case, the LHS is an IN record
    if (rhs->getSchema () == nullptr) {
        rhAtt = rhs->getAtt (0);
        
        // here, it is a regular data record
    } else {
        rhAtt = rhs->getAtt (whichAttIsOrdering);
    }
    
    // now, build the comparison lambda and return
    if (orderingAttType->promotableToInt ()) {
        return [lhAtt, rhAtt]
        {
            return lhAtt->toInt () < rhAtt->toInt ();
        };
    } else if (orderingAttType->promotableToDouble ()) {
        return [lhAtt, rhAtt]
        {
            return lhAtt->toDouble () < rhAtt->toDouble ();
        };
    } else if (orderingAttType->promotableToString ()) {
        return [lhAtt, rhAtt]
        {
            return lhAtt->toString () < rhAtt->toString ();
        };
    } else {
        cout << "This is bad... cannot do anything with the >.\n";
        exit (1);
    }
}

function <bool ()>  MyDB_BPlusTreeReaderWriter :: buildSortComparator (MyDB_RecordPtr lhs, MyDB_RecordPtr rhs) {
    
    MyDB_AttValPtr lhAtt, rhAtt;
    MyDB_AttValPtr lhPtr = nullptr, rhPtr = nullptr;
    
    // in this case, the LHS is an IN record
    if (lhs->getSchema () == nullptr) {
        lhAtt = lhs->getAtt (0);
        lhPtr = lhs->getAtt (1);
        
        // here, it is a regular data record
    } else {
        lhAtt = lhs->getAtt (whichAttIsOrdering);
    }
    
    // in this case, the LHS is an IN record
    if (rhs->getSchema () == nullptr) {
        rhAtt = rhs->getAtt (0);
        rhPtr = rhs->getAtt (1);
        
        // here, it is a regular data record
    } else {
        rhAtt = rhs->getAtt (whichAttIsOrdering);
    }
    
    // now, build the comparison lambda and return
    if (orderingAttType->promotableToInt ()) {
        return [lhAtt, rhAtt, lhPtr, rhPtr]
        {
            if (lhAtt->toInt () == rhAtt->toInt () &&
                lhPtr != nullptr &&
                rhPtr != nullptr)
                return lhPtr->toInt() > rhPtr->toInt();
            
            return lhAtt->toInt () < rhAtt->toInt ();
        };
    } else if (orderingAttType->promotableToDouble ()) {
        return [lhAtt, rhAtt, lhPtr, rhPtr]
        {
            if (lhAtt->toDouble () == rhAtt->toDouble () &&
                lhPtr != nullptr &&
                rhPtr != nullptr)
                return lhPtr->toInt() > rhPtr->toInt();
            
            return lhAtt->toDouble () < rhAtt->toDouble ();
        };
    } else if (orderingAttType->promotableToString ()) {
        return [lhAtt, rhAtt, lhPtr, rhPtr]
        {
            if (lhAtt->toString () == rhAtt->toString () &&
                lhPtr != nullptr &&
                rhPtr != nullptr)
                return lhPtr->toInt() > rhPtr->toInt();
            
            return lhAtt->toString () < rhAtt->toString ();
        };
    } else {
        cout << "This is bad... cannot do anything with the >.\n";
        exit (1);
    }
}


#endif
