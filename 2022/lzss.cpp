// lzss.cpp

/*

2022 Re-factored LZSS.C

MIT License

Copyright (c) 2022 Matt Seabrook

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

//===========================================================================

const int historyBufferSize = 4096;
const int maxMatchLength = 18;
const int threshold = 2;
const int nil = historyBufferSize;

unsigned long int textsize = 0;                                                                           // text size counter
unsigned long int codesize = 0;                                                                           // code size counter
unsigned long int printcount = 0;                                                                         // counter for reporting progress every 1K bytes
unsigned char text_buf[historyBufferSize + maxMatchLength - 1];                                           // ring buffer of size N, with extra F-1 bytes to facilitate string comparison
int match_position, match_length;                                                                         // of longest match.  These are set by the InsertNode() procedure.
int leftChild[historyBufferSize + 1], rightChild[historyBufferSize + 257], parent[historyBufferSize + 1]; // left & right children & parents -- These constitute binary search trees.

std::ifstream inFile;
std::ofstream outFile; // input & output files

//===========================================================================

/*
================
InitTree

    For i = 0 to N - 1, rson[i] and lson[i] will be the right and
    left children of node i.  These nodes need not be initialized.
    Also, dad[i] is the parent of node i.  These are initialized to
    NIL (= N), which stands for 'not used.'
    For i = 0 to 255, rson[N + i + 1] is the root of the tree
    for strings that begin with character i.  These are initialized
    to NIL.  Note there are 256 trees.
================
*/
void InitTree(void) // initialize trees
{
    int i;

    for (i = historyBufferSize + 1; i <= historyBufferSize + 256; i++)
        rightChild[i] = nil;
    for (i = 0; i < historyBufferSize; i++)
        parent[i] = nil;
}

/*
================
InsertNode

Inserts string of length F, text_buf[r..r+F-1], into one of the
trees (text_buf[r]'th tree) and returns the longest-match position
and length via the global variables match_position and match_length.
If match_length = F, then removes the old node in favor of the new
one, because the old one will be deleted sooner.
Note r plays double role, as tree node and position in buffer.
================
*/
void InsertNode(int r)
{
    int i, p, cmp;
    unsigned char *key;

    cmp = 1;
    key = &text_buf[r];
    p = historyBufferSize + 1 + key[0];
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

/*
================
DeleteNode

deletes node p from tree
================
*/
void DeleteNode(int p)
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

/*
================
Encode

Encodes from input file to output file.
================
*/
void Encode(void)
{
    int i, c, len, r, s, last_match_length, code_buf_ptr;
    unsigned char code_buf[17], mask;

    code_buf[0] = 0;         // code_buf[1..16] saves eight units of code, and
    code_buf_ptr = mask = 1; // code_buf[0] works as eight flags, "1" representing that the unit is an unencoded letter (1 byte), "0" a position-and-length pair (2 bytes).
    s = 0;
    r = historyBufferSize - maxMatchLength;
    for (i = s; i < r; i++)
        text_buf[i] = ' '; // Clear the buffer with any character that will appear often.
    for (len = 0; len < maxMatchLength && (c = inFile.get()) != EOF; len++)
        text_buf[r + len] = c; // Read F bytes into the last F bytes of the buffer
    if ((textsize = len) == 0)
        return; // text of size zero
    for (i = 1; i <= maxMatchLength; i++)
        InsertNode(r - i); // Insert the F strings, each of which begins with one or more 'space' characters.  Note the order in which these strings are inserted.  This way,
    // degenerate trees will be less likely to occur.
    InsertNode(r); // Finally, insert the whole string just read.  The global variables match_length and match_position are set.
    do
    {
        if (match_length > len)
            match_length = len; // match_length may be spuriously long near the end of text.
        if (match_length <= threshold)
        {
            match_length = 1;                       // Not long enough match.  Send one byte.
            code_buf[0] |= mask;                    // 'send one byte' flag
            code_buf[code_buf_ptr++] = text_buf[r]; // Send uncoded.
        }
        else
        {
            code_buf[code_buf_ptr++] = (unsigned char)match_position;                                                      // Send position and
            code_buf[code_buf_ptr++] = (unsigned char)(((match_position >> 4) & 0xf0) | (match_length - (threshold + 1))); // length pair. Note match_length > THRESHOLD.
        }
        if ((mask <<= 1) == 0) // Shift mask left one bit.
        {
            for (i = 0; i < code_buf_ptr; i++) // Send at most 8 units of code together
                outFile.put(code_buf[i]);
            code_buf[0] = 0;
            code_buf_ptr = mask = 1;
        }
        last_match_length = match_length;
        for (i = 0; i < last_match_length && (c = inFile.get()) != EOF; i++)
        {
            DeleteNode(s);   // Delete old strings and
            text_buf[s] = c; // read new bytes
            if (s < maxMatchLength - 1)
                text_buf[s + historyBufferSize] = c; // If the position is near the end of buffer, extend the buffer to make string comparison easier.
            s = (s + 1) & (historyBufferSize - 1);
            r = (r + 1) & (historyBufferSize - 1); // Since this is a ring buffer, increment the position modulo the buffer size.
            InsertNode(r);                         // Register the string in text_buf[r..r+F-1]
        }
        if ((textsize += i) > printcount)
        {
            std::cout << std::fixed << std::setprecision(12) << textsize << std::endl; // printf("%12ld\r", textsize);
            printcount += 1024;
        }
        while (i++ < last_match_length) // After the end of text, no need to read, but buffer may not be empty.
        {
            DeleteNode(s); // Delete old strings
            s = (s + 1) & (historyBufferSize - 1);
            r = (r + 1) & (historyBufferSize - 1);
            if (--len)
                InsertNode(r); // If the position is near the end of buffer, extend the buffer to make string comparison easier.
        }
    } while (len > 0);    // until length of string to be processed is zero
    if (code_buf_ptr > 1) // Send remaining code.
        for (i = 0; i < code_buf_ptr; i++)
            outFile.put(code_buf[i]);
    std::cout << "In : " << textsize << " bytes\n";                             // printf("In : %ld bytes\n", textsize); // Encoding is done.
    std::cout << "Out: " << outFile.tellp() << " bytes" << std::endl;           // printf("Out: %ld bytes\n", outFile.tellp());
    std::cout << "Out/In: " << (double)outFile.tellp() / textsize << std::endl; // printf("Out/In: %.3f\n", (double)outFile.tellp() / textsize);
}

/*
================
Decode

Decodes from input file to output file.
================
*/
void Decode(void)
{
    int i, j, k, r, c;
    unsigned int flags;

    for (i = 0; i < historyBufferSize - maxMatchLength; i++)
        text_buf[i] = ' ';
    r = historyBufferSize - maxMatchLength;
    flags = 0;
    for (;;)
    {
        if (((flags >>= 1) & 256) == 0)
        {
            if ((c = inFile.get()) == EOF)
                break;
            flags = c | 0xff00; // uses higher byte cleverly to count eight
        }                       //    'flags' is a 16-bit unsigned integer.
        if (flags & 1)
        {
            if ((c = inFile.get()) == EOF)
                break;
            outFile.put(c);
            text_buf[r++] = c;
            r &= (historyBufferSize - 1);
        }
        else
        {
            if ((i = inFile.get()) == EOF)
                break;
            if ((j = inFile.get()) == EOF)
                break;
            i |= ((j & 0xf0) << 4);
            j = (j & 0x0f) + threshold;
            for (k = 0; k <= j; k++)
            {
                c = text_buf[(i + k) & (historyBufferSize - 1)];
                outFile.put(c);
                text_buf[r++] = c;
                r &= (historyBufferSize - 1);
            }
        }
    }
}

/*

================
helpText

Prints help text.
================
*/
void helpText(void)
{
    std::cout << "\tUsage: lzss [-e|-d] infile outfile\n"
              << std::endl;
}

/*
=============================================================================
                         MAIN ENTRYPOINT
=============================================================================
*/
int main(int argc, char *argv[])
{
    std::cout << "  _     _________ ____  \n"
                 " | |   |__  / ___/ ___| \n"
                 " | |     / /\\___ \\___ \\ \n"
                 " | |___ / /_ ___) |__) |\n"
                 " |_____/____|____/____/ \n"
              << std::endl;
    std::cout << "Lempel-Ziv-Storer-Szymanski (LZSS) compression algorithm" << std::endl;
    std::cout << "11/21/2022 by Matt Seabrook\n"
              << std::endl;

    if (argc != 4)
    {
        helpText();
        return 0;
    }

    inFile.open(argv[2], std::ios::binary);
    if (!inFile.is_open())
    {
        std::cout << "Error: Could not open input file " << argv[2] << std::endl;
        return 0;
    }
    outFile.open(argv[3], std::ios::binary);
    if (!outFile.is_open())
    {
        std::cout << "Error: Could not create output file " << argv[3] << std::endl;
        return 0;
    }

    if (std::string(argv[1]) == "-e")
    {
        Encode();
    }
    else if (std::string(argv[1]) == "-d")
    {
        Decode();
    }
    else
    {
        helpText();
        return 0;
    }

    inFile.close();
    outFile.close();

    return 0;
}