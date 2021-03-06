#pragma once
#include <stdexcept>
#include <limits>
#include <vector>

#include "random.hpp"

enum Movement: char {  UP, DOWN, LEFT, RIGHT };
enum class GameStatus: char { NONE, WIN, LOST };

template <typename U = unsigned char, typename UU = unsigned short>
class SEngine {

	static_assert(sizeof(UU) > sizeof(U), "generic typename UU is <= U, this can be dangerous");
	static_assert(!std::is_signed<U>::value && !std::is_signed<UU>::value, "unsigned values only");

	struct Cell {
		U x, y; 
		bool operator==(const Cell &c) const noexcept { return x == c.x && y == c.y; }
		bool operator!=(const Cell &c) const noexcept { return !(*this == c); }
	};

	public:
		explicit SEngine(U xsz, U ysz, U lv_food, UU score);
		SEngine(const SEngine &) = delete;
		SEngine(SEngine &&) = delete;
		~SEngine();

		SEngine & operator=(const SEngine &) = delete;
		SEngine & operator=(SEngine &&) = delete;


		UU get_score() const noexcept { return score; }
		U  get_food()  const noexcept { return lv_food; }

		GameStatus move(Movement mv);
		const wchar_t * to_wstr(bool isFirstCall = false) noexcept;

	private:
		signed char offset[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
		std::vector<Cell*> snake;

		U xsz, ysz, lv_food;
		UU score;

		Cell food;
		Movement prv_mv;

		wchar_t *buffer; /* wchar representation of the game */
		size_t length;


	private:
		Cell generate() const noexcept { 
			return Cell {rand_in_range((U)0, (U)(xsz-1)), rand_in_range((U)0, (U)(ysz-1))}; 
		}

		inline bool find(const Cell &b) const noexcept {
			for (Cell *a : snake)
				if (*a == b) return true;
			return false;
		}

		void addnewfood() noexcept {
		retry:
			this->food = generate();
			for (Cell *c : snake)
				if (*c == food) goto retry;
		}

		bool isInvalidMovement(const Cell &head, Movement mv) const noexcept {
			return  head.x+offset[mv][0] <= -1 || head.x+offset[mv][0] >= xsz || 
				head.y+offset[mv][1] <= -1 || head.y+offset[mv][1] >= ysz;
		}

		static bool inline areOpposite(Movement a, Movement b) noexcept {
			return ((a == UP && b == DOWN) || (a == DOWN && b == UP)) 
				|| ((a == LEFT && b == RIGHT) || (a == RIGHT && b == LEFT));
		}


};


template <typename U, typename UU>
SEngine<U,UU>::SEngine(U xsz, U ysz, U lv_food, UU score)
       	: xsz{xsz}, ysz{ysz}, lv_food{lv_food}, score{score}, buffer{NULL} {

	if (xsz < 9 || ysz < 9)
		throw std::runtime_error("board size too small");

	if (lv_food <= 0)
		throw std::runtime_error("food must to be at least 1");

	snake.push_back(new Cell{(U)(xsz/2), (U)(ysz/2)});
	snake.push_back(new Cell{(U)(xsz/2), (U)(ysz/2+1)});

	addnewfood();
	prv_mv = LEFT; // default orientation
	to_wstr(true);

}

template <typename U, typename UU>
SEngine<U,UU>::~SEngine() {
	for (Cell *c : snake)
		delete c;

	free(buffer);
}


template <typename U, typename UU>
GameStatus SEngine<U,UU>::move(Movement mv) {

	mv = areOpposite(mv, prv_mv) ? prv_mv : mv;

	if (isInvalidMovement(*snake.front(), mv))
		return GameStatus::LOST;

	// new head
	Cell hold (*snake.front()), tmp;
	snake.front()->x += offset[mv][0];
	snake.front()->y += offset[mv][1];

	// check that it doesn't crash against itself
	int occurrence = 0;
	for (Cell *c : snake) {
		if (*c == *snake.front()) 
			occurrence++;
	}

	if (occurrence > 1)
		return GameStatus::LOST;

	// each cell except the head, takes the place of the previous
	// move the tail of snake
	for (Cell *c : snake) {
		if (*c == *snake.front()) 
			continue;

		tmp  = *c;
		*c   = hold;
		hold = tmp;
	}

	if (*snake.front() == food) {

		snake.push_back(new Cell(hold));
		score++;
		addnewfood();

		if (--lv_food == 0)
			return GameStatus::WIN;
	}

	prv_mv = mv;
	return GameStatus::NONE;

}


/* horrible but fast, however should be rewritten, we draw borders and newline only the first time
to avoid making too many calls to I/O functions we write on memory and for the same reason (overhead) we don't call
std functions like wstringstream */

template <typename U, typename UU>
const wchar_t * SEngine<U,UU>::to_wstr(bool isFirstCall) noexcept {

	size_t k = ((ysz+1) << 1)+3;
	Cell hold;
	int x, y;

	if (isFirstCall) goto _draw_all;

	for (x = 0; x < xsz; x++) {
		for (y = 0; y < ysz; y++) {

			hold.x = x;
			hold.y = y;

			if (y == 0) k+=2;

			if (*snake.front() == hold) {
				buffer[k++] = L'\u2588';
				buffer[k++] = L'\u2588';
			} else if (find(hold)) {
				buffer[k++] = L'\u2588';
				buffer[k++] = L'\u2588';
			} else if (hold == food) {
				buffer[k++] = L'\u25cf';
				buffer[k++] = L' ';
			} else {
				buffer[k++] = L' ';
				buffer[k++] = L' ';
			}

			if (y == ysz-1) k+=3;
		}
	}
	return buffer;

_draw_all:

	// we need of +2 extra rows and cols for borders: (xsz+2)*(ysz+2)
	// we use 2 character for representing each element: (ysz+2)*(ysz+2)*2
	// we have one newline for each row (including borders): +(xsz+2)

	// with xsz=20, ysz=40:  22*42*2+22

#define LENGTH(_XSZ_,_YSZ_) (((_XSZ_+2)*(_YSZ_+2) << 1) + _XSZ_ + 2)

	// big buf for wstring representation, + 1 for L'\0'
	this->buffer = (wchar_t *)malloc( ((this->length = LENGTH(xsz, ysz)) + 1) * sizeof(wchar_t));
#undef LENGTH

	// 2 * (ysz+1) + 2+1
	for (k = 0, y = 0; y <= ysz; y++) {
		buffer[k++] = L'\u2592';
		buffer[k++] = L'\u2592';
	}

	buffer[k++] = L'\u2592';
	buffer[k++] = L'\u2592';
	buffer[k++] = L'\n';

	// xsz*ysz*2 + (xsz*(2+2+1))
	for (x = 0; x < xsz; x++) {
		for (y = 0; y < ysz; y++) {

			if (y == 0) {
				buffer[k++] = L'\u2592';
				buffer[k++] = L'\u2592';
			}

			k+=2;

			if (y == ysz-1) {
				buffer[k++] = L'\u2592';
				buffer[k++] = L'\u2592';
				buffer[k++] = L'\n';
			}
		}
	}

	for (y = 0; y <= ysz; y++) {
		buffer[k++] = L'\u2592';
		buffer[k++] = L'\u2592';
	}

	buffer[k++] = L'\u2592';
	buffer[k++] = L'\u2592';
	buffer[k++] = L'\n';
	buffer[k]   = L'\0';
	return buffer;

	// 2*41+2+1 + 40*20*2 + (20*(2+2+1)) + 2*41+2+1 = 1870
	// +1 (newline) final *2 is because every cell is represented with 2 character
	// throw std::runtime_error( " should be equal: "+std::to_string(k)+" == "+ std::to_string(length));
#undef ISHEAD
}



#if 0
// reassuring but slow
template <typename U, typename UU>
std::wstring SEngine<U,UU>::to_wstr() const {

#define ISHEAD(N) ((N == *snake[0]))

	U x, y;
	std::wstringstream ws;
	const static wchar_t *blk = L"\u2592\u2592";

	for (y = 0; y <= ysz; y++)
		ws << blk;
	ws << blk << L'\n';

	for (x = 0; x < xsz; x++) {
		for (y = 0; y < ysz; y++) {

			if (y == 0) ws << blk;

			if (ISHEAD(board[x][y]))
				ws << L"\u2588\u2588";
			else if (board[x][y].isSnake())
				ws << L"\u2588\u2588";
			else if (board[x][y].isFood())
				ws << L"\u25cf ";
			else
				ws << L"  ";

			if (y == ysz-1)
				ws << blk << L'\n';
		}
	}

	for (y = 0; y <= ysz; y++)
		ws << blk;
	ws << blk << L'\n';

#undef ISHEAD
	return ws.str();
}
#endif
