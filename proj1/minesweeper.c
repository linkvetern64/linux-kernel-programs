/* YOUR FILE-HEADER COMMENT HERE */
/*
	 * unsigned vs signed : read and write ret valu is 
	 * ssize_t signed size_t, represent # of bytes in an allocation
	 * %z modifier prints size_t. %zd. 
	 * 
	 * subtraction underflow, is when unsigned variable - unsigned variable, cant be negative.
	 * the result would be like 981928391823 instead of -5. 
	 * 
	 * 
	 * treat it like a C string. character array.  Char * is a character array.
	 * a pointer to some number of characters
	 * can't treat as strings, they're not null terminated.  Cannot use string functions 
	 * none of them will work and kernel will crash.  They're byte pointers,
	 * can index into like 

	 * Have to use a MMAP to deal with userspace 
	 * user_view is a dynamically allocated 4096 bytes. 
	 * only 1st 100 bytes have meaning.  
	 * 101 should be '\0'
	 * how does those 100 bytes get to game board?
     
	 * byteoffset equation:
     * y * #cols + x
	 * row# * # of elements in column + col # 

 	 * mmap -> for user_view, 
	 * ms and ms_ctl are interfaces to test
	 * you have to memory map it. 

	 * classic mistake: 
	 * if you are revealing mine at location 0,0,
	 * how many surrounding squares are there. 
	 * if you go out of bounds in kernel, kernel crashes.
	 *

	 * Multiple users could write to gameboard or CMD line at same time.
	 
	 * Spin lock examples in lecture
	 */



/*
 * This file uses kernel-doc style comments, which is similar to
 * Javadoc and Doxygen-style comments.  See
 * ~/linux/Documentation/kernel-doc-nano-HOWTO.txt for details.
 */

/*
 * Getting compilation warnings?  The Linux kernel is written against
 * C89, which means:
 *  - No // comments, and
 *  - All variables must be declared at the top of functions.
 * Read ~/linux/Documentation/CodingStyle to ensure your project
 * compiles without warnings.
 */

#define pr_fmt(fmt) "MS: " fmt

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <asm/uaccess.h>

#define NUM_ROWS 10
#define NUM_COLS 10
#define BOARD_SIZE (NUM_ROWS * NUM_COLS)
#define NUM_MINES 10



/** true if using a fixed initial game board, false to randomly
    generate it */
static bool fixed_mines;

/** holds locations of mines */
static bool game_board[NUM_ROWS][NUM_COLS];

/** buffer that displays what the player can see */
static char *user_view;

/** tracks number of mines that the user has marked */
static unsigned mines_marked;

/** true if user revealed a square containing a mine, false
    otherwise */
static bool game_over;

/**
 * String holding the current game status, generated via scnprintf().
 * Game statuses are like the following: "3 out of 10 mines
 * marked" or "Game Over".
 */
static char game_status[80];


static void game_reveal_mines(void);

static void game_reset(void);

/*FIX DIS*/
static void game_reveal_mines(){
	int x, y, pos;
	for(y = 0; y < 10; y++){
		for(x = 0; x < 10; x++){
			pos = 10 * y + x;
			if(game_board[x][y]){
				user_view[pos] = '*';
			}
		}
	}
}


static void game_reset(){
	int i,k, j, marked, X, Y;
	char rand[8];
	//Need \0 null terminator denotes string 
	strncpy(game_status, "Game reset\0", 80);

	i = 0;
	game_over = false;
	mines_marked = 0;

	/* Reset gameboard */
	for(k = 0; k < 10; k++){
		for(j = 0; j < 10; j++){
			game_board[k][j] = false;
		}
	}

	//Fill user_view with . up to 4096 or PAGE_SIZE
	while(i < PAGE_SIZE){ user_view[i++] = '.'; }

	marked = 0;
	fixed_mines = false;
	if(fixed_mines){
		for(i = 0; i < 10; i++){
			game_board[i][i] = true;
		}
	}
	else{
		for(;;){
			get_random_bytes(rand, 1);
			X = abs((int)rand[0]) % 10;	
			get_random_bytes(rand, 1);
			Y = abs((int)rand[0]) % 10;	
			if(!game_board[X][Y]){
				game_board[X][Y] = true;
				marked++;
				if(marked >= 10){
					break;
				}
			}
		}
	}	
}

/**
 * ms_read() - callback invoked when a process reads from /dev/ms
 * @filp: process's file object that is reading from this device (ignored)
 * @ubuf: destination buffer to store game board
 * @count: number of bytes in @ubuf
 * @ppos: file offset
 *
 * Copy to @ubuf the contents of @user_view, starting from the offset
 * @ppos. Copy the lesser of @count and (@BOARD_SIZE - *@ppos). Then
 * increment the value pointed to by @ppos by the number of bytes that
 * were copied.
 *
 * Return: number of bytes written to @ubuf, or negative on error
 */
static ssize_t ms_read(struct file *filp, char __user * ubuf,
		       size_t count, loff_t * ppos)
{
	int len;
	size_t comp;

	len = 100;
	
	if(*ppos != 0){
		return 0;
	}

	comp = (BOARD_SIZE - *ppos);
	count = min(count, comp);

	if(copy_to_user(ubuf, user_view, count)){
		return -EINVAL;
	};

	*ppos = count;
	
	return len;
}

/**
 * ms_mmap() - callback invoked when a process mmap()s to /dev/ms
 * @filp: process's file object that is mapping to this device (ignored)
 * @vma: virtual memory allocation object containing mmap() request
 *
 * Create a read-only mapping from kernel memory (specifically,
 * @user_view) into user space.
 *
 * Code based upon
 * <a href="http://bloggar.combitech.se/ldc/2015/01/21/mmap-memory-between-kernel-and-userspace/">http://bloggar.combitech.se/ldc/2015/01/21/mmap-memory-between-kernel-and-userspace/</a>
 *
 * You do not need to modify this function.
 *
 * Return: 0 on success, negative on error.
 */
static int ms_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);
	unsigned long page = vmalloc_to_pfn(user_view);
	if (size > PAGE_SIZE)
		return -EIO;
	vma->vm_pgoff = 0;
	vma->vm_page_prot = PAGE_READONLY;
	if (remap_pfn_range(vma, vma->vm_start, page, size, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

/**
 * ms_ctl_read() - callback invoked when a process reads from
 * /dev/ms_ctl
 * @filp: process's file object that is reading from this device (ignored)
 * @ubuf: destination buffer to store game board
 * @count: number of bytes in @ubuf
 * @ppos: file offset
 *
 * Copy to @ubuf the contents of @game_status, starting from the
 * offset @ppos. Copy the lesser of @count and (string length of
 * @game_status - *@ppos). Then increment the value pointed to by
 * @ppos by the number of bytes that were copied.
 *
 * Return: number of bytes written to @ubuf, or negative on error
 */
static ssize_t ms_ctl_read(struct file *filp, char __user * ubuf, size_t count,
			   loff_t * ppos)
{
	int len;
	size_t comp;

	len = 80;
	
	if(*ppos != 0){
		return 0;
	}
	//strlength of gamestatus
	comp = (80 - *ppos);
	count = min(count, comp);

	if(copy_to_user(ubuf, game_status, count)){
		return -EINVAL;
	};

	*ppos = count;
	
	return len;
}

/**
 * ms_ctl_write() - callback invoked when a process writes to
 * /dev/ms_ctl
 * @filp: process's file object that is writing to this device (ignored)
 * @ubuf: source buffer from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * Copy the contents of @ubuf, up to the lesser of @count and 8 bytes,
 * to a temporary variable. Then parse that character array as
 * following:
 *
 *   s   - Quit existing game and start a new one.
 *   rXY - Reveal the contents at (X, Y). X and Y must be integers
 *         from zero through nine.
 *   mXY - Toggle the marking at (X, Y) as either a bomb or not. X and
 *         Y must be integers from zero through nine.
 *   q   - Reveal all mines and quit the current game.
 *
 * If the input is none of the above, then return -EINVAL.
 *
 * Be aware that although the user using (X, Y) coordinates,
 * internally your program operates via row/column.
 *
 * WARNING: Note that @ubuf is NOT a string! You CANNOT use strcpy()
 * or strlen() on it!
 *
 * Return: @count, or negative on error
 */
static ssize_t ms_ctl_write(struct file *filp, const char __user * ubuf,
			    size_t count, loff_t * ppos)
{
	int x, y, pos, mines;
	char op;
	
	// PARAM INFO.
	//ubuf is what takes the users input
	//count is size of input + 1 ?for null terminator?
	//ppos && filp are ignored for this.
	//WARNING!!!! -- when the game is ended and picked back up
	//the board stays the same.
	//Check if entry is greater than 3, or character not in lowercase range
	if(count > 4 || !((int)ubuf[0] >= 97 && (int)ubuf[0] <= 122)){
		return -EINVAL;
	}
	op = ubuf[0];
	//Converts XY to integer value			
	x = ubuf[1] - '0';
	y = ubuf[2] - '0';
	
	//Position = 10 * row position + column #
	pos = 10 * y + x;

	/* ONCE THE GAME IS OVER SET OP TO NOTHING */
	/*if(game_over){
		op = ' ';
	}*/
	/* CHECK AGAINST UPPER AND LOWER */
	/* IF !GAME_OVER */
	switch(op){
		case 's':
			printk("Quit and start new\n");
			game_reset();
			/* CODE HERE */
			break;
		case 'r':
			if(game_over){return count;}
			printk("Reveal (X,Y)\n");
			strncpy(game_status, "Revealing pieces\0", 80);

			//Checks if X & Y are non negative and 0 - 9
			if(!((x > -1 && x < 10) && (y > -1 && y < 10))){
				return -EINVAL;
			}
			/* CHECK THAT X & Y in correct positions*/
			else if(game_board[x][y]){
				/* GAME FAILS */
				printk("Hit a mine boiii\n");
				strncpy(game_status, "Game over\0", 80);
				user_view[pos] = '*';
				game_over = true;
			}

			mines = 0;
			//Inner boundaries X&Y no the outer edge
			//check wraparound boundaries
			if((x < 9 && x > 0) && (y > 0 && y < 9)){
				//if(game_board[x][y]){mines++;}
				if(game_board[x][y+1]){mines++;}
				if(game_board[x][y-1]){mines++;}
				if(game_board[x-1][y]){mines++;}
				if(game_board[x-1][y+1]){mines++;}
				if(game_board[x-1][y-1]){mines++;}
				if(game_board[x+1][y]){mines++;}
				if(game_board[x+1][y+1]){mines++;}
				if(game_board[x+1][y-1]){mines++;}
				if(mines > 0){
					user_view[pos] = mines + '0';
				}
				else{
					user_view[pos] = '-';
				}
			}
			else{
				/* CHECK CRUST MINES */
				user_view[pos] = mines + '0';
			}
			break;
		case 'm':
			if(game_over){return count;}

			printk("Marking (X,Y)\n");
			//Checks if X & Y are non negative and 0 - 9
			if(!((x > -1 && x < 10) && (y > -1 && y < 10))){
				return -EINVAL;
			}
			
			//Position = 10 * row position + column #
			pos = 10 * y + x;

			switch(user_view[pos]){
				case '.':
					user_view[pos] = '?';
					break;
				case '?':
					user_view[pos] = '.';
					break;
				default:
					/* Do nothing */
					break;
			}

			break;
		case 'q':
			printk("Quit game\n");
			/* CODE HERE */
			game_reveal_mines();
			break;
		default:
			printk("Not a valid entry\n");
			/* CODE HERE */
			break;
	}

	/*copy_from_user to get user input, to put from user space to kernel space*/
	return count;
}

static const struct file_operations fop_ms = {
	.owner = THIS_MODULE,
	.read = ms_read,
	.mmap = ms_mmap,
};

static const struct file_operations fop_ms_ctl = {
	.owner = THIS_MODULE,
	.read = ms_ctl_read,
	.write = ms_ctl_write,
};

static struct miscdevice ms = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ms",
	.fops = &fop_ms,
	.mode = 0444,
};
static struct miscdevice ms_ctl = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ms_ctl",
	.fops = &fop_ms_ctl,
	.mode = 0666,
};

/**
 * minesweeper_init() - entry point into the minesweeper kernel module
 * Return: 0 on successful initialization, negative on error
 */
static int __init minesweeper_init(void)
{
	pr_info("Initializing the game.\n");
	if (fixed_mines)
		pr_info("Using a fixed minefield.\n");
	user_view = vmalloc(PAGE_SIZE);
	if (!user_view) {
		pr_err("Could not allocate memory\n");
		return -ENOMEM;
	}
	/* YOUR CODE HERE */

	misc_register(&ms);
	misc_register(&ms_ctl);

	game_reset();


 
	
	return 0;
}

/**
 * minesweeper_exit() - called by kernel to clean up resources
 */
static void __exit minesweeper_exit(void)
{
	pr_info("Freeing resources.\n");
	vfree(user_view);

	/* YOUR CODE HERE */
	misc_deregister(&ms);
	misc_deregister(&ms_ctl);
}

module_init(minesweeper_init);
module_exit(minesweeper_exit);
module_param(fixed_mines, bool, 0444);

MODULE_DESCRIPTION("CS421 Minesweeper Game");
MODULE_LICENSE("GPL");
