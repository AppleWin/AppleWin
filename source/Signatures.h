#pragma once

#ifndef DEBUG_SIGS
#define DEBUG_SIGS
#endif // !DEBUG_SIGS

// Number of items in the running program's signature array
#define PROG_SIG_LEN			4

void loadSignatureFile();								// Loads the file with known signatures
void addPageToSigCalculation(UINT page);				// Adds a page to be used potentially in the sig calculation
void calculateProgramSig();								// Calculates the running program's possible signatures
														// and matches them to known signatures
void displaySigCalcs();									// Debug function to display the calculated signatures