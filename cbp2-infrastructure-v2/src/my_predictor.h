// my_predictor.h
// This file contains a sample my_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.
// Note that this predictor doesn't use the whole 32 kilobytes available
// for the CBP-2 contest; it is just an example.

class my_update : public branch_update {
public:
	unsigned int index;
};

class my_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	24
#define TABLE_BITS	28
	my_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];
	unsigned int BHT[65536] = {0};
	int BHT_index;
	unsigned int two_level_history[4096] = {0};
	int two_level_history_index;
	unsigned int predictor_selector[4096] = {0};
	int predictor_selector_index;
	bool gshare_choice;
	bool bimodal_choice;

	my_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			predictor_selector_index = b.address & (4096-1); // get lower 12 bits (since 4096 entries)

			// gshare implementation
			u.index = 
				(history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1)); // xor makes index reliant on history and address
			gshare_choice = tab[u.index] >> 1; // extract the most significant bit of the counter

			// bimodal implementation
			two_level_history_index = b.address & (4096-1); // get lower 12 bits (since 4096 entries)
			bimodal_choice = BHT[two_level_history[two_level_history_index]&(65536-1)] >= 2; // use counter to determine prediction

			if (predictor_selector[predictor_selector_index] < 2) { // use gshare
				u.direction_prediction (gshare_choice);
			}
			else { // else use bimodal
				u.direction_prediction (bimodal_choice);
			}
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			// update predictor selector
			if ((gshare_choice == taken) && (bimodal_choice != taken)) { // gshare right, bimodal wrong -> decrement
				if (predictor_selector[predictor_selector_index] > 0) {
					predictor_selector[predictor_selector_index]--;
				}
			}
			if ((bimodal_choice == taken) && (gshare_choice != taken)) { // bimodal right, gshare wrong -> increment
				if (predictor_selector[predictor_selector_index] < 3) {
					predictor_selector[predictor_selector_index]++;
				}
			}

			// bimodal implementation
			if (taken && (BHT[two_level_history[two_level_history_index]&(65536-1)] < 3)) { // if less than 3
				BHT[two_level_history[two_level_history_index]&(65536-1)]++; // increment if taken
			}
			else if (!taken && (BHT[two_level_history[two_level_history_index]&(65536-1)] > 0)) { // if greater than 0
				BHT[two_level_history[two_level_history_index]&(65536-1)]--; // decrement if not taken
			}

			two_level_history[two_level_history_index] <<= 1; // update local history
			two_level_history[two_level_history_index] |= taken;

			// gshare implementation
			unsigned char *c = &tab[((my_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++; // if taken, increment counter if possible
			} else {
				if (*c > 0) (*c)--; // if not taken, decrement counter if possible
			}
			history <<= 1;
			history |= taken; // update history to include most recent behavior
			history &= (1<<HISTORY_LENGTH)-1;
		}
	}
};