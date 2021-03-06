/**
 * @Author Joshua Standiford (jstand1@umbc.edu)
 * @E-mail jstand1@umbc.edu
 * This file is used to create and insert misc devices into the kernel.
 * /dev/ms will contain the Minesweeper gameboard and /dev/ms_ctl
 * handles user input and prints the game status.
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
#include <linux/spinlock.h>

#define NUM_ROWS 10
#define NUM_COLS 10
#define BOARD_SIZE (NUM_ROWS * NUM_COLS)
#define NUM_MINES 10

/*Spinlock to handle critical sections when modifying board*/
static DEFINE_SPINLOCK(lock);

/** true if using a fixed initial game board, false to randomly
    generate it */
static bool fixed_mines;

/** holds locations of mines */
static bool game_board[NUM_ROWS][NUM_COLS];

/** buffer that displays what the player can see */
static char *user_view;

/** buffer that holds values passed into ms_ctl_write*/
static char *buf;

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

/* @Name: game_reveal_mines
 * @Return: void
 * @Desc: this function reveals the mines as asterisks
 * on the user_view, displayed from /dev/ms
 */
static void game_reveal_mines()
{
	int x, y, pos;
	for (y = 0; y < 10; y++) {
		for (x = 0; x < 10; x++) {
			pos = 10 * y + x;
			if (game_board[x][y]) {
				user_view[pos] = '*';
			}
		}
	}
}

/* @Name: game_reset
 * @Return: void
 * @Desc: This function resets the gameboard.
 * the game_status is reset, game_over is reset
 * random mines are set unless fixed_mines is true.
 */
static void game_reset()
{
	int i, k, j, marked, X, Y;
	char rand[8];
	//Need \0 null terminator denotes string 
	strncpy(game_status, "Game reset\0", 80);

	i = 0;
	game_over = false;
	mines_marked = 0;

	/* Reset gameboard */
	for (k = 0; k < 10; k++) {
		for (j = 0; j < 10; j++) {
			game_board[k][j] = false;
		}
	}

	//Fill user_view with . up to 4096 or PAGE_SIZE
	while (i < PAGE_SIZE) {
		user_view[i++] = '.';
	}

	marked = 0;

	if (fixed_mines) {
		for (i = 0; i < 10; i++) {
			game_board[i][i] = true;
		}
	} else {
		for (;;) {
			get_random_bytes(rand, 1);
			X = abs((int)rand[0]) % 10;
			get_random_bytes(rand, 1);
			Y = abs((int)rand[0]) % 10;
			if (!game_board[X][Y]) {
				game_board[X][Y] = true;
				marked++;
				if (marked >= 10) {
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

	if (*ppos != 0) {
		return 0;
	}

	comp = (BOARD_SIZE - *ppos);
	count = min(count, comp);

	if (copy_to_user(ubuf, user_view, count)) {
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

	if (*ppos != 0) {
		return 0;
	}
	//strlength of gamestatus
	comp = (80 - *ppos);
	count = min(count, comp);

	if (copy_to_user(ubuf, game_status, count)) {
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
	int x, y, pos, mines, i, j, marked_correctly;
	char op;
	size_t comp;
	spin_lock(&lock);
	comp = 8;
	count = min(count, comp);
 	
	if (copy_from_user(buf, ubuf, count)) {
		spin_unlock(&lock);
		return count;
	}
	
	//ubuf is what takes the users input
	//count is size of input + 1 ?for null terminator?
	//ppos && filp are ignored for this
	//Check if entry is greater than 3, or character not in lowercase range
	if (count > 4 || !((int)buf[0] >= 97 && (int)buf[0] <= 122)) {
		spin_unlock(&lock);
		return count;
	}
	op = buf[0];
	//Converts XY to integer value                  
	x = buf[1] - '0';
	y = buf[2] - '0';
	
	//Position = 10 * row position + column #
	pos = 10 * y + x;
	
	switch (op) {
	case 's':
		
		/* More than 's' was entered */
		if((int)buf[1] != 10){
			scnprintf(game_status, 80, "Invalid entry");
			spin_unlock(&lock);
			return count;
		}
		 
		game_reset();
		break;

	case 'r':

		if (game_over) {
			spin_unlock(&lock);
			return count;
		}
		
		if((int)buf[3] != 10){
			scnprintf(game_status, 80, "Invalid entry");
			spin_unlock(&lock);
			return count;
		}

		scnprintf(game_status, 80, "%d Marked of 10", mines_marked);
		//Checks if X & Y are non negative and 0 - 9
		if (!((x > -1 && x < 10) && (y > -1 && y < 10))) {
			spin_unlock(&lock);
			return count;
		}

		/* CHECK THAT X & Y in correct positions */
		else if (game_board[x][y]) {
			strncpy(game_status, "You lose!\0", 80);
			game_reveal_mines();
			game_over = true;
			spin_unlock(&lock);
			return count;
		}

		mines = 0;

		//Inner boundaries X&Y no the outer edge
		//check wraparound boundaries
		if (!game_over && (x < 9 && x > 0) && (y > 0 && y < 9)) {
			if (game_board[x][y + 1]) {
				mines++;
			}
			if (game_board[x][y - 1]) {
				mines++;
			}
			if (game_board[x - 1][y]) {
				mines++;
			}
			if (game_board[x - 1][y + 1]) {
				mines++;
			}
			if (game_board[x - 1][y - 1]) {
				mines++;
			}
			if (game_board[x + 1][y]) {
				mines++;
			}
			if (game_board[x + 1][y + 1]) {
				mines++;
			}
			if (game_board[x + 1][y - 1]) {
				mines++;
			}
			if (mines > 0) {
				user_view[pos] = mines + '0';
			} else {
				user_view[pos] = '-';
			}
		} else if (!game_over) {

			mines = 0;
			/*Check that X and Y are in range of 0 - 9 */
			if (x < 0 || x > 9 || y < 0 || y > 9) {
				spin_unlock(&lock);
				return count;
			}

			if (y + 1 <= 9 && y + 1 >= 0 && game_board[x][y + 1]) {
				mines++;
			}
			if (y - 1 <= 9 && y - 1 >= 0 && game_board[x][y - 1]) {
				mines++;
			}
			if (x - 1 <= 9 && x - 1 >= 0 && game_board[x - 1][y]) {
				mines++;
			}
			if (x - 1 <= 9 && x - 1 >= 0 && y + 1 <= 9 && y + 1 >= 0
			    && game_board[x - 1][y + 1]) {
				mines++;
			}
			if (x - 1 <= 9 && x - 1 >= 0 && y - 1 <= 9 && y - 1 >= 0
			    && game_board[x - 1][y - 1]) {
				mines++;
			}
			if (x + 1 <= 9 && x + 1 >= 0 && game_board[x + 1][y]) {
				mines++;
			}
			if (x + 1 <= 9 && x + 1 >= 0 && y + 1 <= 9 && y + 1 >= 0
			    && game_board[x + 1][y + 1]) {
				mines++;
			}
			if (x + 1 <= 9 && x + 1 >= 0 && y - 1 <= 9 && y - 1 >= 0
			    && game_board[x + 1][y - 1]) {
				mines++;
			}
			if (mines > 0) {
				user_view[pos] = mines + '0';
			} else {
				user_view[pos] = '-';
			}
		}
		break;

	case 'm':
		if (game_over) {
			spin_unlock(&lock);
			return count;
		}
		if((int)buf[3] != 10){
			scnprintf(game_status, 80, "Invalid entry");
			spin_unlock(&lock);
			return count;
		}
		
		//Checks if X & Y are non negative and 0 - 9
		if (!((x > -1 && x < 10) && (y > -1 && y < 10))) {
			spin_unlock(&lock);
			return count;
		}

		switch (user_view[pos]) {
		case '.':
			user_view[pos] = '?';
			mines_marked++;
			break;
		case '?':
			user_view[pos] = '.';
			mines_marked--;
			break;
		default:
			/* Do nothing */
			break;
		}
		marked_correctly = 0;
		for (i = 0; i < 10; i++) {
			for (j = 0; j < 10; j++) {
				pos = 10 * i + j;
				if (game_board[i][j] && user_view[pos] == '?') {
					marked_correctly++;
				}
			}
		}
		scnprintf(game_status, 80, "%d Marked of 10", mines_marked);
		if (marked_correctly == NUM_MINES && mines_marked == NUM_MINES) {
			strncpy(game_status, "Game won!\0", 80);
			game_over = true;
		}

		break;
	case 'q':
		
		if((int)buf[1] != 10 && !game_over){
			scnprintf(game_status, 80, "Invalid entry");
			spin_unlock(&lock);
			return count;
		}

		/* User quits the game */
		strncpy(game_status, "You lose!\0", 80);
		game_over = true;
		game_reveal_mines();
		break;

	default:
		if(game_over){
			spin_unlock(&lock);
			return count;
		}
		scnprintf(game_status, 80, "Invalid entry");
		spin_unlock(&lock);
		return -EINVAL;
		break;
	}
	spin_unlock(&lock);
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
	buf = vmalloc(PAGE_SIZE);
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
	vfree(buf);
	/* YOUR CODE HERE */
	misc_deregister(&ms);
	misc_deregister(&ms_ctl);
}

module_init(minesweeper_init);
module_exit(minesweeper_exit);
module_param(fixed_mines, bool, 0444);

MODULE_DESCRIPTION("CS421 Minesweeper Game");
MODULE_LICENSE("GPL");
