// run threads all at once: should see their pids smeared.
#include "fs/fs.h"
#include "os.h"

#include "rpi.h"
#include "user-progs/byte-array-0-printk-hello.h"

void notmain(void) {
  eqx_verbose(1);

  eqx_config_t c = {.ramMB = 256, .vm_use_pin_p = 1, .vm_use_pt_p = 1};
  eqx_init_config(c);

  pi_sd_init();
  printk("Reading the MBR.\n");
  mbr_t *mbr = mbr_read();

  printk("Loading the first partition.\n");
  mbr_partition_ent_t partition;
  memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
  assert(mbr_part_is_fat32(partition.part_type));

  printk("Loading the FAT.\n");
  fat32_fs_t fs = fat32_mk(&partition);

  printk("Loading the root directory.\n");
  pi_dirent_t root = fat32_get_root(&fs);

  printk("Listing files:\n");
  uint32_t n;
  pi_directory_t files = fat32_readdir(&fs, &root);

  printk("Got %d files.\n", files.ndirents);
  for (int i = 0; i < files.ndirents; i++) {
    if (files.dirents[i].is_dir_p) {
      printk("\tD: %s (cluster %d)\n", files.dirents[i].name,
             files.dirents[i].cluster_id);
    } else {
      printk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name,
             files.dirents[i].cluster_id, files.dirents[i].nbytes);
    }
  }
  printk("Looking for config.txt.\n");
  char *name = "CONFIG.TXT";
  pi_dirent_t *config = fat32_stat(&fs, &root, name);
  demand(config, "config.txt not found!\n");

  printk("Reading config.txt.\n");
  pi_file_t *file = fat32_read(&fs, &root, name);
  printk("Printing config.txt:\n");
  printk("Printing config.txt (%d bytes):\n", file->n_data);
  printk("--------------------\n");
  for (int i = 0; i < file->n_data; i++) {
    printk("%c", file->data[i]);
  }
  printk("--------------------\n");

  // Read HELLO.BIN from SD card
  char *filename = "0-PRIN~4.BIN";
  pi_file_t *hello_file = fat32_read(&fs, &root, filename);
  if (!hello_file) {
    panic("Could not find %s on SD card\n", filename);
  }
  output("Read %s: %d bytes\n", filename, hello_file->n_data);
  printk("hello_file->data: %s\n", hello_file->data);

  // Create a program structure for the loaded file
  struct prog hello_prog = {
      .name = filename, .nbytes = hello_file->n_data,
      // The code field is a flexible array, so we copy the data after it
  };
  printk("hello_prog: %s, %d\n", hello_prog.name, hello_prog.nbytes);

  // Allocate memory for the program structure plus the code data
  struct prog *p = kmalloc(sizeof(struct prog) + hello_file->n_data);
  memcpy(p, &hello_prog, sizeof(struct prog));
  memcpy(p->code, hello_file->data, hello_file->n_data);

  struct prog *p2 = &bytes_0_printk_hello;

  output("about to load: <%s>\n", p2->name);

  let th = eqx_exec_internal(p2);

  output("about to run\n");
  eqx_run_threads();
}
