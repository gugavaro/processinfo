#include <mach-o/dyld_images.h>
#include <mach-o/swap.h>
#include <vector>

unsigned char *readProcessMemory(task_t t, mach_vm_address_t addr,
                                 mach_msg_type_number_t *size) {
  mach_msg_type_number_t dataCnt = (mach_msg_type_number_t)*size;
  vm_offset_t readMem;

  kern_return_t error = vm_read(t,         // vm_map_t target_task,
                                addr,      // mach_vm_address_t address,
                                *size,     // mach_vm_size_t size
                                &readMem,  // vm_offset_t *data,
                                &dataCnt); // mach_msg_type_number_t *dataCnt

  if (error) {
    printf("Unable to read target task's memory @%p - kr 0x%x\n", (void *)addr,
           error);
    return nullptr;
  }

  return ((unsigned char *)readMem);
}

void printMachoHeader(pid_t id, mach_port_t task) {

  // Print UUID
  {
    struct task_extmod_info extmod_info;
    mach_msg_type_number_t count = TASK_EXTMOD_INFO_COUNT;
    if (task_info(task, TASK_EXTMOD_INFO, (task_info_t)&extmod_info, &count) ==
        KERN_SUCCESS) {

      printf(
          "UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%"
          "02x%02x\n",
          extmod_info.task_uuid[0], extmod_info.task_uuid[1],
          extmod_info.task_uuid[2], extmod_info.task_uuid[3],
          extmod_info.task_uuid[4], extmod_info.task_uuid[5],
          extmod_info.task_uuid[6], extmod_info.task_uuid[7],
          extmod_info.task_uuid[8], extmod_info.task_uuid[9],
          extmod_info.task_uuid[10], extmod_info.task_uuid[11],
          extmod_info.task_uuid[12], extmod_info.task_uuid[13],
          extmod_info.task_uuid[14], extmod_info.task_uuid[15]);
    }
  }

  struct task_dyld_info dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  kern_return_t error =
      task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count);

  if (error) {
    printf("%d -> %x [%d - %s]\n", id, task, error, mach_error_string(error));
    return;
  }

  struct dyld_all_image_infos *allImageInfos =
      (struct dyld_all_image_infos *)dyld_info.all_image_info_addr;

  printf("Version: %d, %d images at offset %p\n", allImageInfos->version,
         allImageInfos->infoArrayCount, allImageInfos->infoArray);

  mach_header_64 *header =
      (mach_header_64 *)allImageInfos->infoArray->imageLoadAddress;

  switch (header->magic) {
  case MH_MAGIC:
    printf("Header MH_MAGIC\n");
    break;

  case MH_MAGIC_64:
    printf("Header MH_MAGIC_64\n");
    break;

  default:

    printf("Header Error\n");
    return;
  }

  mach_msg_type_number_t size =
      sizeof(struct dyld_image_info) * allImageInfos->infoArrayCount;

  uint8_t *info_addr = readProcessMemory(
      task, (mach_vm_address_t)allImageInfos->infoArray, &size);

  std::vector<dyld_image_info> dyldImageInfoList;
  dyldImageInfoList.resize(allImageInfos->infoArrayCount);
  memcpy(dyldImageInfoList.data(), info_addr,
         sizeof(struct dyld_image_info) * allImageInfos->infoArrayCount);

  for (const auto &dyldImageInfo : dyldImageInfoList) {
    printf("Path: %s\n", dyldImageInfo.imageFilePath);
  }
}

int main(int argc, char *argv[]) {
  task_t task;
  pid_t id = getpid();
  if (argc > 1) {
    id = atoi(argv[1]);
  }

  kern_return_t error = task_for_pid(mach_task_self(), id, &task);
  if (error) {
    printf("%d -> %x [%d - %s]\n", id, task, error, mach_error_string(error));
    return -1;
  }

  printMachoHeader(id, task);
}
