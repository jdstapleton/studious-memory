#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 32
#define MAX_LINES 1024
#define SIM_CALLS 4

typedef struct sReportData {
    unsigned short line_len;
    unsigned int diagReport[MAX_LINES]; // sizeof(type) here must equal MAX_LINE_LENGTH
    // 4kb buffer for column sums
    unsigned short columnSum[MAX_LINE_LENGTH];
    unsigned short no_lines;
    unsigned short gamma;
    unsigned short epsilon;
} ReportData;

char PB_BUFF[SIM_CALLS][MAX_LINE_LENGTH] = { 0 };
uint8_t CUR_CALL = 0;

void printBits(unsigned int num, unsigned short size){
    int i;
    for(i = size - 1; i>= 0; i--){
        printf("%u",(num >>i) & 1 );
    }
}

char* sprintBits(unsigned int num, unsigned short size){
    char* pbBuff = PB_BUFF[CUR_CALL];
    CUR_CALL = (CUR_CALL + 1) % SIM_CALLS;
    int i;
    for(i = size - 1; i>= 0; i--){
        sprintf(&pbBuff[size - i - 1], "%u",(num >>i) & 1);
    }
    return pbBuff;
}


void printReport(ReportData* report) {
    for (unsigned short i = 0; i < report->no_lines; ++i) {
        printf(": [%02d] %s ", report->diagReport[i], sprintBits(report->diagReport[i], report->line_len));
        if (i % 8 == 7) {
            printf("\n");
        }
    }
    printf("\n");
}

unsigned short getLineLength(FILE* file) {
  char line[MAX_LINE_LENGTH] = {0};
  char* result = fgets(line, MAX_LINE_LENGTH, file);
  rewind(file);
  // newline = 1 char
  return result ? strlen(line) - 1 : MAX_LINE_LENGTH + 1;
}

int readReport(
    char* path, 
    ReportData* report) {

    FILE *file = fopen(path, "r");
    
    if (!file)
    {
        perror(path);
        return EXIT_FAILURE;
    }
    
    char line[MAX_LINE_LENGTH] = {0};
    report->line_len = getLineLength(file);
    // Get each line until there are none left
    while (fgets(line, MAX_LINE_LENGTH, file))
    {
        // get all bits as number values, and sum up each column
        for (int i = 0; i < report->line_len; ++i) {
            // convert to integer value 0 or 1, via subtracting character '0'
            bool val = line[i] - '0';
            report->diagReport[report->no_lines] |= val << (report->line_len - 1 - i);
            report->columnSum[i] += val;
        }
        report->no_lines++;
    }
    
    if (fclose(file))
    {
        return EXIT_FAILURE;
        perror(path);
    }

    if (report->no_lines == 0) {
        fprintf(stderr, "No lines read in exiting...\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void calcGamaEpsilon(ReportData* report) {
    for (int i = 0; i < report->line_len; ++i) {
        // The most common bit by using the feature
        // of integer division dropping decimals, and 
        // doubleing our value. (double works
        // cause we have only 2 possible values in 
        // our domain 0 or 1.)
        // when we shift our bit over to the position repersented (so left most bit is first)
        // then add it to our integer via OR'ing it
        report->gamma |= ((report->columnSum[i] * 2) / report->no_lines) << (report->line_len - 1 - i);
    }
    // Flip all bits to get epsilon.  
    // So get me all 1s for the bitmask to eliminate the extra bits in the integer. (we aren't a full int)
    int mask = (1 << report->line_len) - 1;
    report->epsilon = ~report->gamma & mask;
    printf("gamma=%d, epsilon=%d, product=%d\n", report->gamma, report->epsilon, report->gamma * report->epsilon);
}

void printMasks(unsigned short o2_mask, unsigned short co2_mask, unsigned short match_mask, ReportData* report) {
    printf("Masks o2_mask: [%d] %s; co2_mask: [%d] %s; match_mask: [%d] %s\n", 
        o2_mask, sprintBits(o2_mask, report->line_len), 
        co2_mask, sprintBits(co2_mask, report->line_len),
        match_mask, sprintBits(match_mask, report->line_len));
}

void calcO2CO2_read_wrong(ReportData* report) {
    unsigned short max_mask = (1 << report->line_len) - 1;
    unsigned short o2Level = 0;
    unsigned short co2Level = 0;

    // we want the longest match for both gamma(o2Level) and epsilon(co2Level)
    for (unsigned short match_mask = max_mask; match_mask > 0; match_mask = (match_mask << 1) & max_mask) {
        // create a sub-mask to test against each report
        unsigned short o2_mask = (report->gamma & (match_mask));
        unsigned short co2_mask = (report->epsilon & (match_mask));
        // printMasks(o2_mask, co2_mask, match_mask, report);

        unsigned short o2_match = 0xFFFF;
        unsigned short co2_match = 0xFFFF;

        // now see if we have any matches.  If we do that is our number for that one.
        for (unsigned short i = 0; i < report->no_lines; ++i) {
            // printf("%d-%d ", ((report->diagReport[i] & match_mask) == o2_mask), (report->diagReport[i] & match_mask) == co2_mask);
            if ( o2_match == 0xFFFF && ((report->diagReport[i] & match_mask) == o2_mask))   o2_match = i;
            if (co2_match == 0xFFFF && ((report->diagReport[i] & match_mask) == co2_mask)) co2_match = i;
        }
        // printf("\n");

        if (!o2Level && o2_match != 0xFFFF) {
            o2Level = report->diagReport[o2_match];
        }
        if (!co2Level && co2_match  != 0xFFFF) {
            co2Level = report->diagReport[co2_match];
        }
        if (o2Level && co2Level) {
            break; // exit early
        }
    }

    printf("WRONG: O2 Level: %d, CO2 Level: %d; Product: %d\n", o2Level, co2Level, o2Level * co2Level);
}

void calcO2CO2(ReportData* report) {
    unsigned short max_mask = (1 << report->line_len) - 1;

    // XXX - this is way bigger than it needs to be, I could have 
    // a bit field to represent max_lines.
    bool matchesO2[MAX_LINES] = { [0 ... (MAX_LINES-1) ] = 1 };
    bool matchesCO2[MAX_LINES] = { [0 ... (MAX_LINES-1) ] = 1 };
    unsigned short numMatchesO2 = MAX_LINES;
    unsigned short numMatchesCO2 = MAX_LINES;
    // we want the longest match for both gamma(o2Level) and epsilon(co2Level)
    for (unsigned short bitPos = report->line_len - 1; (short)bitPos != -1; --bitPos) {
        unsigned short bitCnt[2] = { 0 };
        unsigned short posMask = 1 << bitPos;
        // printf("Bit selection: %s\n", sprintBits(posMask, report->line_len));
        for (int i = 0; i < report->no_lines; ++i) {
            // matchesO2 is either 1 or 0(true or false) and we only count if its true 
            bitCnt[(report->diagReport[i] & posMask) >> bitPos] += matchesO2[i];
        }
        bool matchBit = bitCnt[1] >= bitCnt[0];
        //printf("\nBitCnt[%d]: %d, %d; matchBit: %d: ", report->line_len - bitPos - 1, bitCnt[0], bitCnt[1], matchBit);
        unsigned short numMatches = 0;
        // the initializer is to skip the loop when we already narrowed our matches to 1.
        for (unsigned short i = (numMatchesO2 > 1) - 1; i < report->no_lines; ++i) {
            matchesO2[i] &= ((report->diagReport[i] & posMask) >> bitPos) == matchBit;
            // if (matchesO2[i]) printf("[%d] %s ", report->diagReport[i], sprintBits(report->diagReport[i], report->line_len));
            numMatches += (matchesO2[i]);
        }
        numMatchesO2 = numMatchesO2 * (numMatches == 0) + numMatches; // keep our count if we are done

        bitCnt[0] = bitCnt[1] = 0;
        for (int i = 0; i < report->no_lines; ++i) {
            // matchesO2 is either 1 or 0(true or false) and we only count if its true 
            bitCnt[(report->diagReport[i] & posMask) >> bitPos] += matchesCO2[i];
        }
        matchBit = bitCnt[0] > bitCnt[1];
        // printf("\nBitCnt[%d]: %d, %d; matchBit: %d: ", report->line_len - bitPos - 1, bitCnt[0], bitCnt[1], matchBit);
        numMatches = 0;
        // the initializer is to skip the loop when we already narrowed our matches to 1.
        for (unsigned short i = (numMatchesCO2 > 1) - 1; i < report->no_lines; ++i) {
            matchesCO2[i] &= ((report->diagReport[i] & posMask) >> bitPos) == matchBit;
            // if (matchesCO2[i]) printf("[%d] %s ", report->diagReport[i], sprintBits(report->diagReport[i], report->line_len));
            numMatches += (matchesCO2[i]);
        }
        numMatchesCO2 = numMatchesCO2 * (numMatches == 0) + numMatches; // keep our count if we are done
    }
    // printf("\n");

    unsigned short o2Level = 0;
    unsigned short co2Level = 0;
    for (int i = 0; i < report->no_lines; ++i) {
        o2Level = o2Level ? o2Level : (report->diagReport[i] * matchesO2[i]);
        co2Level = co2Level ? co2Level : (report->diagReport[i] * matchesCO2[i]);
    }

    printf("O2 Level: %d, CO2 Level: %d; Product: %d\n", o2Level, co2Level, o2Level * co2Level);
}

int main(int argc, char **argv)
{   
    if (argc < 1)
        return EXIT_FAILURE;
    char* path = argv[1];
    ReportData report = { 0 };
    fprintf(stderr, "Sizeof(report) = %lu\n", sizeof(report));
    int readStatus = readReport(path, &report);
    if (readStatus != EXIT_SUCCESS) {
        return readStatus;
    }

    //printReport(&report);
    calcGamaEpsilon(&report); // p1

    // calcO2CO2_read_wrong(&report); // p2, wrong
    calcO2CO2(&report); // p2

    return EXIT_SUCCESS;
}