// lzss.cpp

/*

C-ot

*/

#include <iostream>
#include <fstream>
#include <string>

const int ringBufferSize = 4096;
const int maxMatchLength = 18;
const int threshold = 2;
const int nil = ringBufferSize;

unsigned long int textsize = 0;                                                                  // text size counter
unsigned long int codesize = 0;                                                                  // code size counter
unsigned long int printcount = 0;                                                                // counter for reporting progress every 1K bytes
unsigned char text_buf[ringBufferSize + maxMatchLength - 1];                                     // ring buffer of size N, with extra F-1 bytes to facilitate string comparison
int match_position, match_length;                                                                // of longest match.  These are set by the InsertNode() procedure.
int leftChild[ringBufferSize + 1], rightChild[ringBufferSize + 257], parent[ringBufferSize + 1]; // left & right children & parents -- These constitute binary search trees.

std::ifstream inFile;
std::ofstream outFile;

// Functions

void InitTree(void) // initialize trees
{
    int i;

    // For i = 0 to N - 1, rson[i] and lson[i] will be the right and
    // left children of node i.  These nodes need not be initialized.
    // Also, dad[i] is the parent of node i.  These are initialized to
    // NIL (= N), which stands for 'not used.'
    // For i = 0 to 255, rson[N + i + 1] is the root of the tree
    // for strings that begin with character i.  These are initialized
    // to NIL.  Note there are 256 trees.

    for (i = ringBufferSize + 1; i <= ringBufferSize + 256; i++)
        rightChild[i] = nil;
    for (i = 0; i < ringBufferSize; i++)
        parent[i] = nil;
}

void InsertNode(int r)
// Inserts string of length F, text_buf[r..r+F-1], into one of the
// trees (text_buf[r]'th tree) and returns the longest-match position
// and length via the global variables match_position and match_length.
// If match_length = F, then removes the old node in favor of the new
// one, because the old one will be deleted sooner.
// Note r plays double role, as tree node and position in buffer.
{
    int i, p, cmp;
    unsigned char *key;

    cmp = 1;
    key = &text_buf[r];
    p = ringBufferSize + 1 + key[0];
    rightChild[r] = leftChild[r] = nil;
    match_length = 0;
    for (;;)
    {
        if (cmp >= 0)
        {
            if (rightChild[p] != nil)
                p = rightChild[p];
            else
            {
                rightChild[p] = r;
                parent[r] = p;
                return;
            }
        }
        else
        {
            if (leftChild[p] != nil)
                p = leftChild[p];
            else
            {
                leftChild[p] = r;
                parent[r] = p;
                return;
            }
        }
        for (i = 1; i < maxMatchLength; i++)
            if ((cmp = key[i] - text_buf[p + i]) != 0)
                break;
        if (i > match_length)
        {
            match_position = p;
            if ((match_length = i) >= maxMatchLength)
                break;
        }
    }
    parent[r] = parent[p];
    leftChild[r] = leftChild[p];
    rightChild[r] = rightChild[p];
    parent[leftChild[p]] = r;
    parent[rightChild[p]] = r;
}

void DeleteNode(int p) // deletes node p from tree
{
    int q;

    if (parent[p] == nil)
        return; // not in tree
    if (rightChild[p] == nil)
        q = leftChild[p];
    else if (leftChild[p] == nil)
        q = rightChild[p];
    else
    {
        q = leftChild[p];
        if (rightChild[q] != nil)
        {
            do
            {
                q = rightChild[q];
            } while (rightChild[q] != nil);
            rightChild[parent[q]] = leftChild[q];
            parent[leftChild[q]] = parent[q];
            leftChild[q] = leftChild[p];
            parent[leftChild[p]] = q;
        }
        rightChild[q] = rightChild[p];
        parent[rightChild[p]] = q;
    }
    parent[q] = parent[p];
    if (rightChild[parent[p]] == p)
        rightChild[parent[p]] = q;
    else
        leftChild[parent[p]] = q;
    parent[p] = nil;
}