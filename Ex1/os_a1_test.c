#include "os.h"
#include <assert.h>
#include <stdio.h>
#define MAX_PAGES 0x100000 /*2^20=16^5
                    ^
                    |__ = 1*16^5
*/
int main(){
    int page;
    printf("starting pre-made tester...\n");
	uint32_t pt0 = alloc_page_frame();
	assert(page_table_query(pt0, 0xcafe) == NO_MAPPING);
	page_table_update(pt0, 0xcafe, 0xf00d);
	assert(page_table_query(pt0, 0xcafe) == 0xf00d);
	page_table_update(pt0, 0xcafe, NO_MAPPING);
	assert(page_table_query(pt0, 0xcafe) == NO_MAPPING);
    printf("end of pre made tester.\n\n");
    
    printf("creating 3 more page tables...\n");
    uint32_t pt1 = alloc_page_frame();
    uint32_t pt2 = alloc_page_frame();
    uint32_t pt3 = alloc_page_frame();
    printf("finished.\n\n");

    printf("checking that all vpn's are not mapped...\n");
    for (page=0;page<MAX_PAGES;++page){
        assert(page_table_query(pt0,page)==NO_MAPPING);
        assert(page_table_query(pt1,page)==NO_MAPPING);
        assert(page_table_query(pt2,page)==NO_MAPPING);
        assert(page_table_query(pt3,page)==NO_MAPPING);
    }
    printf("finito.\n\n");

    printf("mapping ppn's to vpn's...\n");
    for (page=0;page<0x0ffff;++page){
        if (page<0x04000){
            page_table_update(pt0,0xa0000 | page,page);
        }
        else if (page<0x08000){
            page_table_update(pt1,0xb0000 | page,page);
        }
        else if (page<0x0c000){
            page_table_update(pt2,0xc0000 | page, page);
        }
        else {
            page_table_update(pt3, 0xd0000 | page, page);
        }
    }
    printf("checking that the mapping is correct...\n");
    for (page=0;page<0x0ffff;++page){
        if (page<0x04000){
            assert(page_table_query(pt0,0xa0000 | page)==page);
        }
        else if (page<0x08000){
            assert(page_table_query(pt1,0xb0000 | page)==page);
        }
        else if (page<0x0c000){
            assert(page_table_query(pt2,0xc0000 | page)==page);
        }
        else {
            assert(page_table_query(pt3,0xd0000 | page)==page);
        }
    }
    printf("donezo.\n\n");

    printf("unmapping everything...\n");
    for(page=0;page<MAX_PAGES;page++){
        page_table_update(pt0,page,NO_MAPPING);
        page_table_update(pt1,page,NO_MAPPING);
        page_table_update(pt2,page,NO_MAPPING);
        page_table_update(pt3,page,NO_MAPPING);
    }
    printf("checking it is indeed unmapped...\n");
    for (page=0;page<MAX_PAGES;++page){
        assert(page_table_query(pt0,page)==NO_MAPPING);
        assert(page_table_query(pt1,page)==NO_MAPPING);
        assert(page_table_query(pt2,page)==NO_MAPPING);
        assert(page_table_query(pt3,page)==NO_MAPPING);
    }
    printf("done and done.\n\n");

    printf("mapping the maximum number of pages to pt0... (2^20 pages)\n");
    for(page=0;page<MAX_PAGES;page++){
        page_table_update(pt0,page,page);
    }
    printf("checking that indeed everything is mapped, and also that the mapping is correct...\n");
    for(page=0;page<MAX_PAGES;page++){
        assert(page_table_query(pt0,page)==page);
    }
    printf("excellente");
    printf("\nThe tester is finsihed and everything is A-Okay! go have fun!\n\n");
    printf(",adPPYba, 88       88  ,adPPYba,  ,adPPYba,  ,adPPYba, ,adPPYba, ,adPPYba,  \n"
 "I8[    \"\" 88       88 a8\"     \"\" a8\"     \"\" a8P_____88 I8[    \"\" I8[    \"\"  \n"
 " `\"Y8ba,  88       88 8b         8b         8PP\"\"\"\"\"\"\"  `\"Y8ba,   `\"Y8ba,   \n"
 "aa    ]8I \"8a,   ,a88 \"8a,   ,aa \"8a,   ,aa \"8b,   ,aa aa    ]8I aa    ]8I  \n"
 "`\"YbbdP\"'  `\"YbbdP'Y8  `\"Ybbd8\"'  `\"Ybbd8\"'  `\"Ybbd8\"' `\"YbbdP\"' `\"YbbdP\"' \n");

	
	return 0;
}
