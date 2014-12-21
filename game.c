#define _GNU_SOURCE
#include <unistd.h>
#include <ncurses.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <stdbool.h>
#include "draw.h"
#include "board.h"

static jmp_buf  jmpbuf;
static sigset_t all_signals;

/* Not sure if it's safe to longjmp from signal handler.
 * Jumps back to main on every signal.
 */
static void sig_handler(int __attribute__((unused))sig_no)
{
	sigprocmask(SIG_BLOCK, &all_signals, NULL);
	longjmp(jmpbuf, 1);
}



int main(void)
{
	if (!isatty(fileno(stdout))) {
		/* not running in terminal */
		exit(0);
	}

	WINDOW *board_win = NULL;
	WINDOW *score_win = NULL;
	board_t board;
	/* use volatile to supress compiler warning:
	 * "variable might be clobbered by longjmp"
	 */
	volatile bool terminal_too_small = false;
	volatile bool gameover = false;
	int score = 0;
	int max_score = 0;
	const struct timespec addsquare_time = {.tv_sec = 0,
		                                .tv_nsec = 100000000};
	setup_screen();
	srand(time(NULL));


	
	if (!load_game(board, &score, &max_score)) {
		board_start(board);
		score = max_score = 0;
	}

	if (init_win(&board_win, &score_win) == WIN_TOO_SMALL) {
		terminal_too_small = true;
		print_too_small();
	} else {
		refresh_board(board_win, board, gameover);
		refresh_score(score_win, score, 0, max_score);
	}


	if (setjmp(jmpbuf) != 0) {
		/* longjmp from sig_handler */
		goto sigint;
	}

	sigfillset(&all_signals);
	signal(SIGINT,  sig_handler);
	signal(SIGABRT, sig_handler);
	signal(SIGTERM, sig_handler);



	int ch;
	while ((ch = getch()) != 'q' && ch != 'Q') {  // q to quit
		/* if terminal's too small do nothing
		   until it's restored */
		if (terminal_too_small && ch != KEY_RESIZE)
			continue;
		int points = 0;
		dir_t dir;
		board_t new_board = {{0}};
		board_t moves     = {{0}};

		switch(ch) {
			case KEY_UP:    dir = UP;    break; // moving
			case KEY_DOWN:  dir = DOWN;  break;
			case KEY_LEFT:  dir = LEFT;  break;
			case KEY_RIGHT: dir = RIGHT; break;

			case 'r': case 'R':               // start new game
				score = 0;
				gameover = false;
				board_start(board);
				refresh_board(board_win, board, gameover);
				refresh_score(score_win, score, 0, max_score);
				continue; // main loop
				break; // too feel safe :)

			case KEY_RESIZE:                  // terminal resize
				if (init_win(&board_win, &score_win) ==
				                            WIN_TOO_SMALL) {
					terminal_too_small = true;
					print_too_small();
					continue;
				} else {
					terminal_too_small = false;
				}
				refresh_board(board_win, board, gameover);
				refresh_score(score_win, score, points, max_score);
				continue;  // main loop
				break;
			default: continue; // main loop
		}

		if (gameover) continue;

		/* block all signals while operating on board */
		sigprocmask(SIG_BLOCK, &all_signals, NULL);

		points = board_slide(board, new_board, moves, dir);
		if (points >= 0) {
			refresh_score(score_win, score, points, max_score);
			draw_slide(board_win, board, moves, dir);

			board_copy(board, new_board);
			score += points;
			if (score > max_score)
				max_score = score;
			refresh_board(board_win, board, gameover);
			refresh_score(score_win, score, points, max_score);

			nanosleep(&addsquare_time, NULL);
			board_add_tile(board, false);
			refresh_board(board_win, board, gameover);
		//didn't slide, check if game's over
		} else if (!board_can_slide(board)) {
			gameover = true;
			refresh_board(board_win, board, gameover);
			refresh_score(score_win, score, points, max_score);
		}
		sigprocmask(SIG_UNBLOCK, &all_signals, NULL);
		flushinp();
	}

	sigprocmask(SIG_BLOCK, &all_signals, NULL);
sigint:
	save_game(board, score, max_score);
	endwin();
	return 0;
}
