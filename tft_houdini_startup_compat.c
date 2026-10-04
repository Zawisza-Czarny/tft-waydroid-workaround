#define _GNU_SOURCE
/* User-authorized procfs-PC override experiment, 2026-10-04.
 * Overwrites only the last token in a narrowly validated read buffer.
 * Does not modify application files, code, comparison flags or state. */
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/user.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#define TASK_MAX 2048
struct task {pid_t tid;int active,stopped,pending_signal,read_pending,status_pending,mmap_pending,pc_index;uint64_t buffer,count,frame,pc,guest_pc,guest_frame,context;};
static struct task tasks[TASK_MAX];
static volatile sig_atomic_t stopping;
static int captured;
static void on_signal(int sig){(void)sig;stopping=1;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static struct task *find(pid_t tid){
 for(int i=0;i<TASK_MAX;i++)if(tasks[i].active&&tasks[i].tid==tid)return &tasks[i];
 for(int i=0;i<TASK_MAX;i++)if(!tasks[i].active){memset(&tasks[i],0,sizeof(tasks[i]));tasks[i].tid=tid;tasks[i].active=1;return &tasks[i];}
 fprintf(stderr,"task table full\n");stopping=1;return NULL;
}
static int is_tft(pid_t tid){char path[80],buf[256]={0};snprintf(path,sizeof(path),"/proc/%d/cmdline",tid);int fd=open(path,O_RDONLY);if(fd<0)return 0;ssize_t n=read(fd,buf,sizeof(buf)-1);close(fd);return n>0&&!strcmp(buf,"com.riotgames.league.teamfighttactics");}
static ssize_t memory(pid_t tid,uint64_t addr,void *out,size_t n){char path[80];snprintf(path,sizeof(path),"/proc/%d/mem",tid);int fd=open(path,O_RDONLY);if(fd<0)return -1;ssize_t r=pread(fd,out,n,(off_t)addr);int saved=errno;close(fd);errno=saved;return r;}
static void dumphex(const char *key,const unsigned char *p,size_t n){printf("%s=",key);for(size_t i=0;i<n;i++)printf("%02x",p[i]);putchar('\n');}
static uint64_t module_base(pid_t tid,uint64_t pc){
 char path[80],line[4096];snprintf(path,sizeof(path),"/proc/%d/maps",tid);FILE *f=fopen(path,"r");uint64_t base=0;if(!f){printf("maps_error=%d\n",errno);return 0;}
 while(fgets(line,sizeof(line),f)){unsigned long long lo,hi,off;char perms[8];if(sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,perms,&off)!=4)continue;
  if(strstr(line,"/libmvg.so")){printf("mvg_map=%s",line);if(off==0&&!base)base=lo;}
  if(pc>=lo&&pc<hi)printf("host_pc_map=%s",line);
 }
 fclose(f);return base;
}
static void snapshot(struct task *t,const char *event){
 if(t->context){uint64_t state[0x350/8];
  if(memory(t->tid,t->context,state,sizeof(state))==(ssize_t)sizeof(state)){
   printf("GUEST_RUNTIME_CONTEXT pc=0x%"PRIx64" lr=0x%"PRIx64" sp=0x%"PRIx64"\n",state[0x310/8],state[30],state[0x308/8]);
   for(int i=0;i<31;i++)printf("guest_x%d=0x%"PRIx64"\n",i,state[i]);
   if(strstr(event,"fatal")){
    uint64_t fp=state[29];
    for(int i=0;i<24 && fp;i++){
     uint64_t pair[2];if(memory(t->tid,fp,pair,16)!=16)break;
     printf("GUEST_FP_FRAME index=%d fp=0x%"PRIx64" previous=0x%"PRIx64" return=0x%"PRIx64"\n",i,fp,pair[0],pair[1]);
     module_base(t->tid,pair[1]);
     if(pair[0]<=fp || pair[0]-fp>0x1000000)break;
     fp=pair[0];
    }
   }
  }
 }

 printf("EVENT=%s tid=%d time=%.9f frame=0x%"PRIx64" proc_pc=0x%"PRIx64"\n",event,t->tid,now(),t->frame,t->pc);
 uint64_t base=module_base(t->tid,t->pc);printf("mvg_base=0x%"PRIx64" expected_guest_pc=0x%"PRIx64"\n",base,base?base+0x53c3a4:0);
 unsigned char bytes[12];if(t->pc>=8&&memory(t->tid,t->pc-8,bytes,12)==12){uint32_t words[3];memcpy(words,bytes,12);dumphex("host_words_bytes",bytes,12);printf("host_words=0x%08x,0x%08x,0x%08x host_sum32=0x%08x\n",words[0],words[1],words[2],words[0]+words[1]+words[2]);}else printf("host_words_error=%d\n",errno);
 if(base){uint64_t g;if(memory(t->tid,base+0x12e4788,&g,8)==8)printf("G=0x%016"PRIx64"\n",g);else printf("G_error=%d\n",errno);
  if(memory(t->tid,base+0x53c39c,bytes,12)==12)dumphex("guest_site_bytes",bytes,12);else printf("guest_site_error=%d\n",errno);
 }
 struct {unsigned off;size_t n;const char *name;} slots[]={{0xdb8,8,"db8"},{0x179c,4,"179c"},{0x102c,4,"102c"},{0x15fc,4,"15fc"},{0xdac,4,"state"}};
 for(unsigned i=0;i<sizeof(slots)/sizeof(slots[0]);i++){uint64_t v=0;if(memory(t->tid,t->frame+slots[i].off,&v,slots[i].n)==(ssize_t)slots[i].n)printf("slot_%s=0x%"PRIx64"\n",slots[i].name,v);else printf("slot_%s_error=%d\n",slots[i].name,errno);}
 puts("END_EVENT");fflush(stdout);
}
static int replace_pc(pid_t tid, uint64_t buffer, size_t n, uint64_t guest) {
    char path[80], text[256], replacement[32];
    uint32_t code[3], expected[3]={0x14000001,0xd4000001,0x14000001};
    if (!n || n>255 || guest<8) return -1;
    snprintf(path,sizeof(path),"/proc/%d/mem",tid);
    int fd=open(path,O_RDWR);
    if(fd<0) return -1;
    if(pread(fd,code,12,(off_t)(guest-8))!=12 || memcmp(code,expected,12) ||
       pread(fd,text,n,(off_t)buffer)!=(ssize_t)n) {close(fd);return -1;}
    text[n]=0;
    char *last=strrchr(text,' ');
    if(!last || strncmp(last+1,"0x",2)) {close(fd);return -1;}
    char *digits=last+3;
    size_t width=strspn(digits,"0123456789abcdefABCDEF");
    if(width<6 || width>16 || (digits[width]!='\n' && digits[width]!=0)) {close(fd);return -1;}
    int len=snprintf(replacement,sizeof(replacement),"%0*llx",(int)width,(unsigned long long)guest);
    if(len!=(int)width) {close(fd);return -1;}
    fprintf(stderr,"OVERRIDE tid=%d original=%.*s replacement=0x%s\n",tid,(int)width+2,last+1,replacement);
    ssize_t written=pwrite(fd,replacement,width,(off_t)(buffer+(uint64_t)(digits-text)));
    close(fd);
    return written==(ssize_t)width?0:-1;
}

static void replace_tracer_pid(struct task *t,size_t n){
 if(!n || n>4096)return;
 char path[80],buf[4097];snprintf(path,sizeof(path),"/proc/%d/mem",t->tid);
 int fd=open(path,O_RDWR);if(fd<0)return;
 if(pread(fd,buf,n,(off_t)t->buffer)!=(ssize_t)n){close(fd);return;}buf[n]=0;
 char *field=strstr(buf,"TracerPid:");if(!field){close(fd);return;}
 char *digits=field+10;while(*digits==' ' || *digits=='\t')digits++;
 size_t width=strspn(digits,"0123456789");
 if(!width || (digits[width]!='\n' && digits[width]!=0)){close(fd);return;}
 printf("TRACERPID_OVERRIDE tid=%d original=%.*s\n",t->tid,(int)width,digits);
 memset(digits,'0',width);
 if(pwrite(fd,digits,width,(off_t)(t->buffer+(uint64_t)(digits-buf)))!=(ssize_t)width)stopping=1;
 close(fd);
}
static void syscall_stop(struct task *t){
 struct ptrace_syscall_info si;memset(&si,0,sizeof(si));if(ptrace(PTRACE_GET_SYSCALL_INFO,t->tid,sizeof(si),&si)<0){perror("GET_SYSCALL_INFO");stopping=1;return;}
 if(si.op==PTRACE_SYSCALL_INFO_ENTRY){t->read_pending=0;t->status_pending=0;
  t->mmap_pending=0;
  if(si.entry.nr==9 && si.entry.args[1]==0xc00000000ULL && si.entry.args[2]==3 && si.entry.args[3]==0x22 && is_tft(t->tid)){
   struct user_regs_struct regs;memset(&regs,0,sizeof(regs));
   if(ptrace(PTRACE_GETREGS,t->tid,0,&regs)<0){stopping=1;return;}
   regs.r10|=0x4000;
   if(ptrace(PTRACE_SETREGS,t->tid,0,&regs)<0){stopping=1;return;}
   t->mmap_pending=1;puts("MMAP_NORESERVE_APPLIED size=0xc00000000 original_flags=0x22 effective_flags=0x4022");
  }
  if(si.entry.nr==257 && is_tft(t->tid)){
   char pathname[384]={0};if(memory(t->tid,si.entry.args[1],pathname,sizeof(pathname)-1)>0)
    printf("TARGET_OPEN tid=%d dirfd=%lld flags=0x%llx path=%s\n",t->tid,(long long)si.entry.args[0],(unsigned long long)si.entry.args[2],pathname);
  }
  if(si.entry.nr==157 && is_tft(t->tid))printf("TARGET_PRCTL tid=%d op=%llu a1=0x%llx a2=0x%llx\n",t->tid,(unsigned long long)si.entry.args[0],(unsigned long long)si.entry.args[1],(unsigned long long)si.entry.args[2]);
  if(si.entry.nr!=0)return;
  char path[100],target[512];snprintf(path,sizeof(path),"/proc/%d/fd/%llu",t->tid,(unsigned long long)si.entry.args[0]);ssize_t n=readlink(path,target,sizeof(target)-1);if(n<0)return;target[n]=0;
  if(is_tft(t->tid) && (!strncmp(target,"/proc/",6) || !strncmp(target,"/sys/",5)))printf("TARGET_READ tid=%d path=%s count=%llu\n",t->tid,target,(unsigned long long)si.entry.args[2]);
  int status_pid=0,status_used=0;
  if(sscanf(target,"/proc/%d/status%n",&status_pid,&status_used)==1 && !target[status_used] && is_tft(t->tid) && is_tft(status_pid)){
   t->status_pending=1;t->buffer=si.entry.args[1];return;
  }
  int pid=0,tid=0;int used=0;if(sscanf(target,"/proc/%d/task/%d/syscall%n",&pid,&tid,&used)!=2||target[used]||tid!=t->tid||!is_tft(t->tid))return;
  t->read_pending=1;t->buffer=si.entry.args[1];t->count=si.entry.args[2];t->guest_pc=0;t->guest_frame=0;
  /* Spoof experiment: use the observed 12-site initialization order.
   * These PCs are inferred from the earlier NDK capture, not sampled
   * Houdini guest registers; verify each original B/SVC/B before writing. */
  static const uint64_t sites[]={0x53c3a4,0x7e696c,0xc7e748,0xd68220,0xd797a0,0xe20628,0x5c8cd8,0x64a438,0x756de4,0xbc90dc,0xc01b50,0xc31938};
  uint64_t base=module_base(t->tid,si.instruction_pointer);
  if(base && t->pc_index<12)t->guest_pc=base+sites[t->pc_index++];
  printf("HOUDINI_INFERRED_PC index=%d pc=0x%"PRIx64"\n",t->pc_index,t->guest_pc);
  printf("MATCH_READ_ENTRY tid=%d path=%s buffer=0x%"PRIx64" count=%"PRIu64" host_ip=0x%llx\n",t->tid,target,t->buffer,t->count,(unsigned long long)si.instruction_pointer);fflush(stdout);
 }else if(si.op==PTRACE_SYSCALL_INFO_EXIT&&t->mmap_pending){
  t->mmap_pending=0;printf("MMAP_RESERVATION_RESULT=0x%llx\n",(unsigned long long)si.exit.rval);
  if(!si.exit.is_error){puts("DETACH_AFTER_RESERVATION");stopping=1;}
 }else if(si.op==PTRACE_SYSCALL_INFO_EXIT&&t->status_pending){
  t->status_pending=0;if(si.exit.rval>0)replace_tracer_pid(t,(size_t)si.exit.rval);
 }else if(si.op==PTRACE_SYSCALL_INFO_EXIT&&t->read_pending){t->read_pending=0;ssize_t n=si.exit.rval;if(n<=0||n>255){printf("read_result=%zd\n",n);return;}
  char buf[256];if(memory(t->tid,t->buffer,buf,(size_t)n)!=n){printf("buffer_error=%d\n",errno);return;}buf[n]=0;char *last=strrchr(buf,' ');if(!last)return;t->pc=strtoull(last+1,NULL,0);t->frame=t->buffer-8;
  printf("READ_RESULT tid=%d length=%zd\n",t->tid,n);dumphex("proc_buffer_hex",(unsigned char *)buf,(size_t)n);snapshot(t,"proc_read_exit");
  if(!t->guest_pc || replace_pc(t->tid,t->buffer,(size_t)n,t->guest_pc)<0){
    fprintf(stderr,"OVERRIDE_SKIPPED_UNMATCHED_SITE tid=%d guest_pc=0x%"PRIx64"\n",t->tid,t->guest_pc);return;
  }
  printf("OVERRIDE_APPLIED tid=%d guest_pc=0x%"PRIx64"\n",t->tid,t->guest_pc);captured++;
 }
}
static int delivery_signal(int status){unsigned event=(unsigned)status>>16;int sig=WSTOPSIG(status);return event||sig==(SIGTRAP|0x80)?0:sig;}
static void detach_all(void){
 for(int i=0;i<TASK_MAX;i++)if(tasks[i].active&&!tasks[i].stopped)ptrace(PTRACE_INTERRUPT,tasks[i].tid,0,0);
 double limit=now()+3;
 while(now()<limit){int remaining=0;for(int i=0;i<TASK_MAX;i++)if(tasks[i].active){if(tasks[i].stopped){if(ptrace(PTRACE_DETACH,tasks[i].tid,0,tasks[i].pending_signal)<0&&errno!=ESRCH)fprintf(stderr,"detach %d: %s\n",tasks[i].tid,strerror(errno));tasks[i].active=0;}else remaining++;}
  if(!remaining)break;
  int status;pid_t tid=waitpid(-1,&status,__WALL|WNOHANG);if(tid>0){struct task *t=find(tid);if(!t)continue;if(WIFEXITED(status)||WIFSIGNALED(status)){t->active=0;continue;}if(WIFSTOPPED(status)){t->stopped=1;t->pending_signal=delivery_signal(status);unsigned event=(unsigned)status>>16;if(event==PTRACE_EVENT_FORK||event==PTRACE_EVENT_VFORK||event==PTRACE_EVENT_CLONE){unsigned long child=0;ptrace(PTRACE_GETEVENTMSG,tid,0,&child);find((pid_t)child);}}}else usleep(1000);
 }
 puts("DETACH_COMPLETE");fflush(stdout);
}
int main(int argc,char **argv){
 if(argc!=2){fprintf(stderr,"usage: observer ZYGOTE_PID\n");return 2;}pid_t root=atoi(argv[1]);if(root<=1)return 2;
 signal(SIGINT,on_signal);signal(SIGTERM,on_signal);setvbuf(stdout,NULL,_IOLBF,0);
 unsigned long opts=PTRACE_O_TRACESYSGOOD|PTRACE_O_TRACEFORK|PTRACE_O_TRACEVFORK|PTRACE_O_TRACECLONE;
 if(ptrace(PTRACE_SEIZE,root,0,opts)<0){perror("SEIZE");return 2;}find(root);if(ptrace(PTRACE_INTERRUPT,root,0,0)<0){perror("INTERRUPT");detach_all();return 2;}
 printf("OBSERVER_READY root=%d pid=%d\n",root,getpid());double deadline=now()+30;
 while(!stopping&&now()<deadline){int status;pid_t tid=waitpid(-1,&status,__WALL|WNOHANG);if(tid<=0){if(tid<0&&errno!=EINTR&&errno!=ECHILD){perror("waitpid");break;}usleep(200);continue;}
  struct task *t=find(tid);if(!t)break;if(WIFEXITED(status)||WIFSIGNALED(status)){t->active=0;continue;}if(!WIFSTOPPED(status))continue;t->stopped=1;t->pending_signal=delivery_signal(status);unsigned event=(unsigned)status>>16;int sig=WSTOPSIG(status);
  if(event==PTRACE_EVENT_FORK||event==PTRACE_EVENT_VFORK||event==PTRACE_EVENT_CLONE){unsigned long child=0;ptrace(PTRACE_GETEVENTMSG,tid,0,&child);find((pid_t)child);}
  else if(sig==(SIGTRAP|0x80))syscall_stop(t);
  else if((sig==SIGSEGV||sig==SIGABRT||sig==SIGBUS||sig==SIGILL)&&!event&&t->frame){siginfo_t si;memset(&si,0,sizeof(si));ptrace(PTRACE_GETSIGINFO,tid,0,&si);struct user_regs_struct r;memset(&r,0,sizeof(r));ptrace(PTRACE_GETREGS,tid,0,&r);printf("ORIGINAL_FATAL_SIGNAL sig=%d tid=%d address=%p code=%d host_rip=0x%llx\n",sig,tid,si.si_addr,si.si_code,r.rip);snapshot(t,"original_fatal_signal");stopping=1;break;}
  if(stopping)break;
  if(event==PTRACE_EVENT_STOP&&sig!=SIGTRAP){if(ptrace(PTRACE_LISTEN,tid,0,0)<0)perror("LISTEN");}
  else if(ptrace(PTRACE_SYSCALL,tid,0,t->pending_signal)<0&&errno!=ESRCH){perror("SYSCALL");stopping=1;}
  t->stopped=0;t->pending_signal=0;
 }
 detach_all();printf("CAPTURED_READS=%d\n",captured);return captured?0:1;
}
