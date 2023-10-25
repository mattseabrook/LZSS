// lzss.cpp

/*

2023 Re-factored LZSS.C

MIT License

Copyright (c) 2023 Matt Seabrook

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
#include <vector>

//===========================================================================

constexpr int historyBufferSize = 4096;		// buffer size of history window
constexpr int maxMatchLength = 18;			// upper limit for match_length
constexpr int threshold = 2;				// encode string into position and length if match_length is greater than this

/*
================
Encode

Encodes from input file to output file.
================
*/
void Encode(std::istream& inFile, std::ostream& outFile) {
	std::vector<uint8_t> text_buf(historyBufferSize + maxMatchLength - 1);
	std::vector<int> parent(historyBufferSize + 1);
	std::vector<int> leftChild(historyBufferSize + 257);
	std::vector<int> rightChild(historyBufferSize + 257);
	constexpr int nil = historyBufferSize;

	//
	// initTree
	//
	auto initTree = [&]() {
		for (int i = historyBufferSize + 1; i <= historyBufferSize + 256; ++i) {
			rightChild[i] = nil;
		}
		for (int i = 0; i < historyBufferSize; ++i) {
			parent[i] = nil;
		}
		};

	//
	// insertNode
	//
	auto insertNode = [&](int r) {
		auto cmp = 1;
		const auto& key = text_buf[r];
		auto p = historyBufferSize + 1 + key;

		rightChild[r] = leftChild[r] = nil;
		match_length = 0;

		while (true) {
			if (cmp >= 0) {
				if (rightChild[p] != nil)
					p = rightChild[p];
				else {
					rightChild[p] = r;
					parent[r] = p;
					return;
				}
			}
			else {
				if (leftChild[p] != nil)
					p = leftChild[p];
				else {
					leftChild[p] = r;
					parent[r] = p;
					return;
				}
			}

			for (int i = 1; i < maxMatchLength; ++i) {
				cmp = key[i] - text_buf[p + i];
				if (cmp != 0)
					break;
			}

			if (int i = maxMatchLength; i > match_length) {
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
		if (rightChild[parent[p]] == p)
			rightChild[parent[p]] = r;
		else
			leftChild[parent[p]] = r;
		parent[p] = nil; // remove p
		};

	//
	// deleteNode
	//
	auto deleteNode = [&](int p) {
		if (parent[p] == nil) return; // p is not in the tree

		int q;

		if (rightChild[p] == nil) {
			q = leftChild[p];
		}
		else if (leftChild[p] == nil) {
			q = rightChild[p];
		}
		else {
			q = leftChild[p];
			if (rightChild[q] != nil) {
				do {
					q = rightChild[q];
				} while (rightChild[q] != nil);

				rightChild[parent[q]] = leftChild[q];
				if (leftChild[q] != nil) {
					parent[leftChild[q]] = parent[q];
				}
				leftChild[q] = leftChild[p];
				if (leftChild[p] != nil) {
					parent[leftChild[p]] = q;
				}
			}
			rightChild[q] = rightChild[p];
			if (rightChild[p] != nil) {
				parent[rightChild[p]] = q;
			}
		}

		parent[q] = parent[p];

		if (rightChild[parent[p]] == p) {
			rightChild[parent[p]] = q;
		}
		else {
			leftChild[parent[p]] = q;
		}

		parent[p] = nil; // p is now removed from the tree
		};


	initTree();

	int match_length = 0;
	int match_position = 0;
	std::vector<uint8_t> code_buf(1 + maxMatchLength);
	int code_buf_ptr = 1;
	int mask = 1;
	int s = 0;
	int r = historyBufferSize - maxMatchLength;
	int len;

	// Copy-Paste the rest of the new refactored code here
}


//Original
void Encode(void)
{
	int i, c, len, r, s, last_match_length, code_buf_ptr;
	unsigned char code_buf[17], mask;

	InitTree(); // initialize trees

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
	{
		InsertNode(r - i);
	}

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
			// std::cout << std::fixed << std::setprecision(12) << textsize << std::endl;
			printf("%12ld\r", textsize);
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
	} while (len > 0); // until length of string to be processed is zero

	if (code_buf_ptr > 1) // Send remaining code.
		for (i = 0; i < code_buf_ptr; i++)
			outFile.put(code_buf[i]);
	// std::cout << "In : " << textsize << " bytes\n";
	printf("In : %ld bytes\n", textsize); // Encoding is done.
	// std::cout << "Out: " << outFile.tellp() << " bytes" << std::endl;
	printf("Out: %ld bytes\n", outFile.tellp());
	// std::cout << "Out/In: " << (double)outFile.tellp() / textsize << std::endl;
	printf("Out/In: %.3f\n", (double)outFile.tellp() / textsize);
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
			flags = c | 0xff00;
		}
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

Prints help text
================
*/
void helpText() {
	std::cout << "Usage:\n"
		<< "  lzss [-e|-d] infile outfile\n\n"
		<< "Options:\n"
		<< "  -e    Encode infile to outfile\n"
		<< "  -d    Decode infile to outfile\n"
		<< std::endl;
}

/*
=============================================================================
						 MAIN ENTRYPOINT
=============================================================================
*/
int main(int argc, char* argv[])
{
	std::cout << "  _     _________ ____  \n"
		" | |   |__  / ___/ ___| \n"
		" | |     / /\\___ \\___ \\ \n"
		" | |___ / /_ ___) |__) |\n"
		" |_____/____|____/____/ \n"
		<< std::endl;
	std::cout << "Lempel-Ziv-Storer-Szymanski (LZSS) compression algorithm" << std::endl;
	std::cout << "10/24/2023 by Matt Seabrook\n"
		<< std::endl;

	if (argc != 4) {
		helpText();
		return EXIT_FAILURE;
	}

	std::ifstream inFile;
	std::ofstream outFile;

	std::string mode = argv[1];
	std::string inputPath = argv[2];
	std::string outputPath = argv[3];

	if (!std::filesystem::exists(inputPath)) {
		std::cerr << "Error: Input file does not exist.\n";
		return EXIT_FAILURE;
	}

	inFile.open(inputPath, std::ios::binary);
	if (!inFile) {
		std::cerr << "Error: Unable to open input file.\n";
		return EXIT_FAILURE;
	}

	outFile.open(outputPath, std::ios::binary);
	if (!outFile) {
		std::cerr << "Error: Unable to open output file.\n";
		return EXIT_FAILURE;
	}

	if (mode == "-e") {
		Encode();
	}
	else if (mode == "-d") {
		Decode();
	}
	else {
		std::cerr << "Error: Unknown mode. Use -e for encode or -d for decode.\n";
		return EXIT_FAILURE;
	}

	inFile.close();
	outFile.close();

	return EXIT_SUCCESS;
}