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
#include <filesystem>
#include <vector>

//===========================================================================

constexpr int historyBufferSize = 4096;		// buffer size of history window
constexpr int maxMatchLength = 18;			// upper limit for match_length
constexpr int threshold = 2;				// match_length must be greater than this

/*
================
Encode

Encodes from input file to output file.
================
*/
void Encode(std::istream& inFile, std::ostream& outFile) {
	std::vector<uint8_t> buffer(historyBufferSize + maxMatchLength - 1);
	std::vector<int> parent(historyBufferSize + 1);
	std::vector<int> leftChild(historyBufferSize + 257);
	std::vector<int> rightChild(historyBufferSize + 257);
	constexpr int nil = historyBufferSize;
	int match_length = 0;
	int match_position = 0;

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
		int cmp = 1;
		const unsigned char* key = &buffer[r];
		int p = historyBufferSize + 1 + buffer[r]; // using buffer[r] as it's the first byte of 'key'.

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

			// Compare sequences, not individual characters.
			for (int i = 1; i < maxMatchLength; ++i) {
				cmp = key[i] - buffer[p + i]; // 'key' is a sequence, so compare elements.
				if (cmp != 0)
					break;
			}

			if (int i = maxMatchLength; i > match_length) {
				match_position = p;
				if ((match_length = i) >= maxMatchLength)
					break;
			}
		}

		// After finding the match, update the tree's links appropriately.
		parent[r] = parent[p];
		leftChild[r] = leftChild[p];
		rightChild[r] = rightChild[p];
		parent[leftChild[p]] = r;
		parent[rightChild[p]] = r;
		if (rightChild[parent[p]] == p)
			rightChild[parent[p]] = r;
		else
			leftChild[parent[p]] = r;
		parent[p] = nil; // Disconnect the matched node/sequence.
		};

	//
	// deleteNode
	//
	auto deleteNode = [&](int p) {
		if (parent[p] == nil) return;

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

		parent[p] = nil;
		};

	initTree();

	std::vector<uint8_t> code_buf(1 + maxMatchLength);
	int code_buf_ptr = 1;
	int mask = 1;
	int s = 0;
	int r = historyBufferSize - maxMatchLength;
	int len;

	for (len = 0; len < maxMatchLength && inFile.peek() != EOF; len++)
		inFile.read(reinterpret_cast<char*>(&buffer[r + len]), 1);

	while (len > 0) {
		if (match_length > len)
			match_length = len;

		if (match_length <= threshold) {
			match_length = 1;
			code_buf[0] |= mask;
			code_buf[code_buf_ptr++] = buffer[r];
		}
		else {

			code_buf[code_buf_ptr++] = static_cast<uint8_t>(match_position);
			code_buf[code_buf_ptr++] = static_cast<uint8_t>(match_length - (threshold + 1));
		}

		if ((mask <<= 1) == 0) {
			for (int i = 0; i < code_buf_ptr; i++)
				outFile.put(code_buf[i]);
			code_buf[0] = 0;
			code_buf_ptr = 1;
			mask = 1;
		}

		int last_match_length = match_length;

		for (int i = 0; i < last_match_length; ++i) {
			if (inFile.peek() != EOF) {
				deleteNode(s);
				inFile.read(reinterpret_cast<char*>(&buffer[s]), 1);

				if (s < maxMatchLength - 1)
					buffer[s + historyBufferSize] = buffer[s];

				s = (s + 1) & (historyBufferSize - 1);
				r = (r + 1) & (historyBufferSize - 1);
				insertNode(r);
			}
			else {
				len--;
			}
		}

		while (len-- > last_match_length) {
			deleteNode(s);
			s = (s + 1) & (historyBufferSize - 1);
			r = (r + 1) & (historyBufferSize - 1);
			if (len > 0) insertNode(r);
		}
	}

	if (code_buf_ptr > 1) {
		for (int i = 0; i < code_buf_ptr; i++)
			outFile.put(code_buf[i]);
	}
}


/*
================
Decode

Decodes from input file to output file.
================
*/
void Decode(std::istream& inFile, std::ostream& outFile) {
	std::vector<uint8_t> buffer(historyBufferSize + maxMatchLength - 1, 0);
	int r = historyBufferSize - maxMatchLength;
	unsigned int flags = 0;

	//std::fill(buffer.begin(), buffer.end(), ' ');	// Clear or initialize the buffer, assuming it's the same size as during encoding.

	while (true) {
		if (((flags >>= 1) & 256) == 0) {
			char c;
			if (!(inFile.get(c))) break;
			flags = static_cast<unsigned char>(c) | 0xff00;
		}

		if (flags & 1) {
			char c;
			if (!(inFile.get(c))) break;
			outFile.put(c);
			buffer[r++] = static_cast<uint8_t>(c);
			r &= (historyBufferSize - 1);
		}
		else {
			int i;
			int j;
			char temp;

			if (!(inFile.get(temp))) break;
			i = static_cast<unsigned char>(temp);

			if (!(inFile.get(temp))) break;
			j = static_cast<unsigned char>(temp);

			i |= ((j & 0xf0) << 4);
			j = (j & 0x0f) + threshold;

			for (int k = 0; k <= j; ++k) {
				char c = buffer[(i + k) & (historyBufferSize - 1)];
				outFile.put(c);
				buffer[r++] = static_cast<uint8_t>(c);
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
	std::cout << "10/25/2023 by Matt Seabrook\n"
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
		Encode(inFile, outFile);
	}
	else if (mode == "-d") {
		Decode(inFile, outFile);
	}
	else {
		std::cerr << "Error: Unknown mode. Use -e for encode or -d for decode.\n";
		return EXIT_FAILURE;
	}

	inFile.close();
	outFile.close();

	return EXIT_SUCCESS;
}