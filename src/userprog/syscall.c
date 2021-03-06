#include <syscall-nr.h>
#include <stdio.h>
#include <string.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
  
static void syscall_handler (struct intr_frame *);

// HELP!!!!!!!!!!!!!! Is this crit. sec too big?

bool valid_mkdir (char *dir, struct dir *dir_to_add);
char * get_cmd_line(char * cmd_line);
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);
void get_last (char * path, char * stop);
struct dir* get_dir (char *dir);

int exec (const char *cmd_line);
struct semaphore sema_files;  // only allow one file operation at a time

//Spencer driving here
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  sema_init (&sema_files, 1);
  dir_path[0] = '/';
}

//Spencer and Jeff driving here
static void
syscall_handler (struct intr_frame *f) 
{  
  // get the system call
  int * esp = f->esp;
  bad_pointer (esp);

  // redirect to the correct function handler
  switch (*esp)
  { 
    case SYS_HALT: // 0
      halt ();
      break;

    case SYS_EXIT: // 1
      bad_pointer (esp+1);
      exit (*(esp+1));  // status
      break;
 
    case SYS_EXEC: // 2  
      bad_pointer (*(esp+1));

      f->eax = exec (*(esp+1));  // cmd_line pointer
      break;
 
    case SYS_WAIT: // 3
      bad_pointer (esp+1);
      f->eax = wait (*(esp+1)); // gets first argument that is pid
      break;  

    case SYS_CREATE:   // 4
      bad_pointer (*(esp+1));
      bad_pointer (esp+2);
      f->eax = create ( *(esp+1), *(esp+2) );  // file char pointer and inital size
      break;

    case SYS_REMOVE:    // 5
       bad_pointer (*(esp+1));
       f->eax = remove (*(esp+1));  // file char pointer
      break;

    case SYS_OPEN:     // 6
      bad_pointer (*(esp+1));   
      f->eax = open (*(esp+1));  // file char pointer
      break;

    case SYS_FILESIZE:  // 7
      bad_pointer (esp+1);
      f->eax = filesize (*(esp+1));  // fd index
      break;

    case SYS_READ:   
      bad_pointer (esp+1);
      bad_pointer (*(esp+2));
      bad_pointer (esp+3);
      f->eax = read ( *(esp+1), *(esp+2), *(esp+3) );  // fd, buffer pointer, size
      break;

    case SYS_WRITE:   
      bad_pointer (esp+1);
      bad_pointer (*(esp+2));
      bad_pointer (esp+3);
      f->eax = write ( *(esp+1), *(esp+2), *(esp+3) ); // fd, buffer pointer, size
      break;
    
    case SYS_SEEK:
      bad_pointer (esp+1);
      bad_pointer (esp+2);
      seek ( *(esp+1), *(esp+2) );
      break;

    case SYS_TELL:
      bad_pointer (esp+1);
      f->eax = tell (*(esp+1)); 
      break;

    case SYS_CLOSE:
      bad_pointer (esp+1);
      close (*(esp+1));
      break;

    case SYS_CHDIR:
   //   bad_pointer (esp+1);
      f->eax = chdir (*(esp+1));
      break;

    case SYS_MKDIR:
    //  bad_pointer (esp+1);
      f->eax = mkdir (*(esp+1));
      break;

    case SYS_READDIR:   
      f->eax = readdir ((esp+1), *(esp+2));
      break;

    case SYS_ISDIR:        
      //bad_pointer (esp+1);
      f->eax = isdir ((esp+1));
      break; 

    case SYS_INUMBER:   
    //  bad_pointer (esp+1);
      f->eax = inumber ((esp+1));
      break;     

    default: 
      thread_exit ();
      break;
  }   
} 

/*Closes all the files in a process, called before it exists */
void
close_files()
{
  int counter;
  for(counter = 2; counter < 130; counter++)
    close (counter);   
}        

// helper to check if pointer is mapped, !null and in the user address space
//Cohen and Spencer driving here
void
bad_pointer(int *esp) 
{
  struct thread *cur = thread_current ();
  
  // check if esp is a valid ptr at all
  if(esp == NULL) //need to check for unmapped
  {
    printf ("%s: exit(%d)\n", cur->name, cur->exit_status);
    thread_exit ();
  }

  // make sure esp is in the user address space
  if(is_kernel_vaddr (esp))
  {
    printf ("%s: exit(%d)\n", cur->name, cur->exit_status);
    thread_exit ();
  }
    
  // check if esp is unmapped
  if( pagedir_get_page (cur->pagedir, esp) == NULL )
  {
    printf ("%s: exit(%d)\n", cur->name, cur->exit_status);
    thread_exit ();
  }
}

//Jeff driving here
void 
halt (void)
{
  shutdown_power_off ();
} 
 
// Cohen driving here
void  
exit (int status)
{
  struct thread *cur = thread_current ();

  cur->exit_status = status;

  char *save_ptr; // for spliter  

  printf ("%s: exit(%d)\n", strtok_r(cur->name, " ", &save_ptr), status);

  file_close (cur->save); // closing file so that write can be allowed

  close_files (); // close all open files in a process
 
  thread_exit (); 
}

// execute the given command line
//Dakota driving here
int 
exec (const char *cmd_line)
{   
  // get string of args and file name, pass into execute
  int tid = process_execute (cmd_line);

  // wait for child to return
  if(tid == -1) // if not loaded correctly
    return -1;
  
  else          // if child loaded correctly
    return tid;      
}

//Spencer driving here
int 
wait (pid_t pid)
{
  return process_wait (pid);  // call process_wait
}

//Dakota driving here
bool 
create (const char *file, unsigned initial_size)
{
  sema_down (&sema_files); // prevent multi-file manipulation
  bool res = filesys_create (file, initial_size);  // save result
  sema_up (&sema_files);   // release file
  return res;


}

//Jeff driving here
bool 
remove (const char *file)
{
  sema_down (&sema_files); // prevent multi-file manipulation
  bool res = filesys_remove (file); // save result
  sema_up (&sema_files); // release file
  return res;
}

//Cohen driving here
int 
open (const char *file)
{

  sema_down (&sema_files); // prevent multi-file manipulation
  
  struct thread *cur = thread_current ();
  cur->file_pointers[cur->fd_index] = filesys_open (file);
 // printf("\n open \n\n");

  // if file is null -1, else return incremented index
  int res = (cur->file_pointers[cur->fd_index] == NULL) ? -1 : cur->fd_index++;
  
  sema_up (&sema_files); // release file
  return res;
} 

//Spencer driving here
int 
filesize (int fd) 
{
  sema_down (&sema_files); // prevent multi-file manipulation

  // if fd is invalid return 0, otherwise get length of file
  int res = (fd<2 || fd>thread_current ()->fd_index) ? 0 : 
            file_length (thread_current ()->file_pointers[fd]);

  sema_up (&sema_files); // release file
  return res;
}

//Jeff and Spencer driving here
int 
read (int fd, void *buffer, unsigned size)
{  
  sema_down (&sema_files); // prevent multi-file manipulation
  struct thread *cur = thread_current ();
  int res; // save result

  // read from keyboard
  if(fd == 0)
  {
    unsigned i;
    for(i = 0; i < size; i++)
    {
      // cast to char
      char * c_ptr = (char *) buffer;
      *(c_ptr+i) = input_getc ();
    }
    res = size;
  }

  // if fd is valid, read the file  
  else if(fd > 1 && fd <= cur->fd_index && buffer != NULL)
    res = file_read (cur->file_pointers[fd], buffer, size);

  else
    res = -1;

  sema_up (&sema_files); // release file
  return res;
}

//Dakota driving here
int 
write (int fd, const void *buffer, unsigned size)
{
  struct thread * cur = thread_current ();


  if(fd > 0 && fd <= cur->fd_index && buffer != NULL)
  {
    sema_down (&sema_files);

    //write to console
    if(fd==1)
    {
      // if there is nothing to write
      if(size == 0 || buffer == NULL)
      {
        sema_up (&sema_files);
        return 0;
      }    
      putbuf (buffer, size);

      sema_up (&sema_files);
      return size;
    }

    // write to file
    if(fd != 0 && fd != 1)
    {
     struct file* temp_file = cur->file_pointers[fd];

      if(temp_file->inode->is_dir == -1)
      {
        sema_up (&sema_files);
        return -1;
      }

      sema_up (&sema_files);
      struct thread *cur = thread_current ();


      return file_write (cur->file_pointers[fd], buffer, size);
    }

    // done writing, let other write
    sema_up (&sema_files);
    return size;
  }
  // bad fd
  return 0;
}

// Spencer driving here
void 
seek (int fd , unsigned position)
{
  sema_down (&sema_files); // prevent multi-file manipulation
  if((fd>=2 && fd<=thread_current ()->fd_index))
  {
    file_seek (thread_current ()->file_pointers[fd], position);
    sema_up (&sema_files); // release file
  }
  else
  {
    sema_up (&sema_files); // release file
    thread_exit ();
  }
}

// Cohen driving here
unsigned  
tell (int fd)
{
  sema_down (&sema_files); // prevent multi-file manipulation

  unsigned res;

  if(fd<2 || fd>thread_current ()->fd_index)
  {
    sema_up (&sema_files); // release file
    thread_exit ();
  }

  res = file_tell (thread_current ()->file_pointers[fd]);
  sema_up (&sema_files); // release file
  return res;
}
 
// Dakota driving here
void 
close (int fd)
{
  sema_down (&sema_files); // prevent multi-file manipulation
  
  struct thread * t = thread_current ();  // get the current thread
  
  if(fd>=2 && fd<=t->fd_index)           // check fd is valid
    if(t->file_pointers[fd] != NULL)     // check that it is not closed
    { 
      file_close (t->file_pointers[fd]); // close the file
     
      // keep closed file from closing
      t->file_pointers[fd] = NULL;      
    }
  // else fail silently
  sema_up (&sema_files); // release file
}

/* Change the current directory. */
// Spencer driving
bool 
chdir (const char *dir)
{
  sema_down (&sema_files); // prevent multi-file manipulation
   struct inode *cur_inode = calloc (1, sizeof (struct inode)); 
  

  // printf("\ngot to chdir of %d\n\n", thread_current()->current_dir->inode->sector);
  // if(dir_lookup(thread_current()->current_dir, dir, cur_inode))
    // printf("\n\n\nIN ROOT %d\n\n\n",cur_inode->sector);


 

  struct dir *curr_dir = get_dir (dir);
  // printf("\n\n\ndir osahdosa: %s sector: %d\n\n\n", dir, curr_dir->inode->sector);
  if(curr_dir->pos == -1)
  {
    free (curr_dir);
    sema_up (&sema_files); // release file
    return false;
  }




  // printf("\n\nbefore: %d curr_dir: %d\n\n", thread_current ()->current_dir->inode->sector, curr_dir->inode->sector);
  // update the current directory
  *(thread_current ()->current_dir) = *curr_dir;
  // printf("\n\nafter: %d\n\n", thread_current ()->current_dir->inode->sector);
  // printf("\n\n\nioihsd: %d\n\n\n", thread_current ()->current_dir->inode->sector);
  sema_up (&sema_files); // release file

  return true;


}

/* Create a directory. */
// Dakota driving
bool 
mkdir (const char *dir)
{
  sema_down (&sema_files); // prevent multi-file manipulation

  if (strcmp(dir, "") == 0)  // if no directory name given
  {
    sema_up (&sema_files); // release file
    return false;
  }

  struct dir * curr_dir = dir_open_root ();
  char *stop;
  get_last (dir, stop); // directory to make

  // search through directories
  char s[strlen(dir)];
  strlcpy(s, dir, strlen (dir)+1);  //moves path copy into s, add 1 for null
  char * token, save_ptr;
  struct inode *cur_inode;

  char s2[strlen(dir)];

  // go into each directory checking for validity
  for (token = strtok_r (s, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
  {
    // check that the last directory doesn't already exist (we are creating it)
    if (strcmp(token, stop) == 0) // HELP: is this a valid directory: hello/hello/hello // maybe just use a counter of the numbers of end stop things and decrement
    {
      // if the directory already exists, return false
      strlcpy(s2, stop, strlen (dir)+1); 

      if (dir_lookup (curr_dir, token, cur_inode))
      {
        curr_dir->empty = false;
        sema_up (&sema_files); // release file
        return false;
      }

      // get an available sector
      block_sector_t sector = 0;
      free_map_allocate (1, &sector);
      dir_create (sector, 128);
      // printf("\n\n\ndirectory %s has sector: %d\n\n\n", dir, sector);

      // printf("\n\ndir: %s sector: %d\n\n", dir, sector);
      // printf("\n\ns2: %s sector: %d\n\n", s2, sector);


      // curr_dir is the parent directory of the directory to make
      dir_add (curr_dir, s2, sector);
      curr_dir->empty = false;
      sema_up (&sema_files); // release file
      return true;
    }

    // make sure the directory exists
    if (!dir_lookup (curr_dir, token, cur_inode))
    {
      curr_dir->empty = false;
      sema_up (&sema_files); // release file
      return false;
    } 

    // go into the next directory
    // printf("\n\n\niusaguiagia \n\n\n");
    curr_dir = dir_open (cur_inode);
  }
  curr_dir->empty = false;
  sema_up (&sema_files); // release file
  return false; // should never get here
}

 /* Reads a directory entry. */
// Jefferson driving
bool 
readdir (int fd, char *name)
{
  sema_down (&sema_files); // prevent multi-file manipulation
  struct file * f_dir = thread_current ()->file_pointers[fd];
  struct dir *new_dir = calloc (1, sizeof (struct dir));
//ASSERT(false);
  new_dir->inode = f_dir->inode;

  new_dir->pos = f_dir->pos;
  sema_up (&sema_files); // release file
  return dir_readdir (new_dir, name);
}

/* Tests if a fd represents a directory. */
// Cohen driving
bool 
isdir (int fd)
{
  struct file * f_dir = thread_current ()->file_pointers[fd];
  return f_dir -> inode -> is_dir == -1;
}

/* Returns the inode number for a fd. */
// Spencer driving
int 
inumber (int fd)
{
  struct file * f_dir = thread_current ()->file_pointers[fd];
  return dir_get_inode (f_dir)-> sector;
}

// gets the last token in the path split by '/'
// Dakota driving
void
get_last (char * path, char *stop)
{
  char *token, *save_ptr;       // for spliter
  char s[strlen(path)];
  char *save_tok;

  strlcpy(s, path, strlen (path)+1);  //moves path copy into s, add 1 for null

  for (token = strtok_r (s, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
  {
    if(token != NULL)
    {    
      save_tok = token;
    }
  }
  char save[strlen(save_tok)];
  strlcpy (save, save_tok, strlen (save_tok)+1); 

  stop = (char*) &save;
}

// return the directory corresponding to passed in dir string
// Jefferson driving
struct dir*
get_dir (char *dir)
{
  struct dir *save_dir = thread_current ()->current_dir;
  struct dir *curr_dir = save_dir; // prevent actual updating
  char *token, save_ptr;
  char s[strlen(dir)];

  // bad dir to return later
  struct dir *bad_dir = calloc (1, sizeof (struct dir));
  bad_dir->pos = -1;
  bad_dir->dir_name = "THIS IS A BAD DIR";

  // copy dir into s (to preserve it) and make sure copy was successful
  if(strlcpy (s, dir, strlen (dir)+1) != strlen (dir))
    return bad_dir;

  if(dir[0] == '/') // absolute, start at the top (root)
  {
    curr_dir = dir_open_root ();
    // printf ("\n\n\ncalling get_dir on root which has sector: %d\n\n\n", 
                                      // curr_dir->inode->sector);
  }
  // printf("\n\n\ncur dir: %d\n\n\n", save_dir->inode->sector);

  // if relative, keep curr_dir where it is and go from there

  struct inode *cur_inode = calloc (1, sizeof (struct inode)); // allocate memory
  // go into each directory checking for validity
  for (token = strtok_r (s, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
  {    

    // printf ("\n\n\ncalling get_dir on %s and curr_dir has sector: %d\n\n\n", 
    //                                   dir, curr_dir->inode->sector);

    if(!dir_lookup (curr_dir, token, cur_inode)) // make sure directory exists
    {
      // printf("\n\nGOT HERE\n\n");
      return bad_dir;
    }

    curr_dir = dir_open (cur_inode); // HELP: need to close each thing opened because malloc (might run out of memory). One idea is a global list of opened stuff.
  }

  free (bad_dir);
  return curr_dir;
}
