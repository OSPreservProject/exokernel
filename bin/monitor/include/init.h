#ifndef _INIT_H_
#define _INIT_H_

void config_defaults(void);
void stdio_init(void);
void register_uidt_handlers(void);
int mem_init(unsigned int startpage, unsigned int numpages);
int low_mem_init(void);
int high_mem_init(void);
void module_init(void);


#endif
