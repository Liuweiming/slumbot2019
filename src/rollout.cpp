// I thought maybe there was a bug.  On postflop streets, we gather WML
// statistics for every hole card pair whether canonical or not.  Do the
// suits of the cards encode information about future cards to come?
// Oh, this is only true if we only iterate through canonical boards.  But
// for the postflop streets we iterate through all raw boards.  For the
// preflop we iterate through only the canonical boards so I had to worry
// about this.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <thread>
#include <vector>

#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "cards.h"
#include "constants.h"
#include "game.h"
#include "hand_value_tree.h"
#include "rollout.h"
#include "sorting.h"

using std::pair;
using std::vector;

static short *RiverHandStrength(const Card *board, bool wins) {
  int max_street = Game::MaxStreet();
  int num_board_cards = Game::NumBoardCards(max_street);

  vector<int> vb(num_board_cards);
  for (int i = 0; i < num_board_cards; ++i) vb[i] = board[i];
  std::sort(vb.begin(), vb.end());
  int *sorted_board = new int[num_board_cards];
  for (int i = 0; i < num_board_cards; ++i) {
    sorted_board[(num_board_cards - 1) - i] = vb[i];
  }
  int num_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  vector<pair<int, int>> v(num_hole_card_pairs);
  int max_card = Game::MaxCard();
  int hole_cards[2];
  int hcp = 0;
  for (int hi = 1; hi <= max_card; ++hi) {
    if (InCards(hi, board, num_board_cards)) continue;
    hole_cards[0] = hi;
    for (int lo = 0; lo < hi; ++lo) {
      if (InCards(lo, board, num_board_cards)) continue;
      hole_cards[1] = lo;
      int hv = HandValueTree::Val(sorted_board, hole_cards);
      int enc = hi * (max_card + 1) + lo;
      v[hcp] = std::make_pair(hv, enc);
      ++hcp;
    }
  }
  delete[] sorted_board;
  sort(v.begin(), v.end(), g_pii_lower_compare);

  int num_enc = (max_card + 1) * (max_card + 1);
  short *values = new short[num_enc];
  // Necessary?
  for (int i = 0; i < num_enc; ++i) values[i] = 32000;
  int num_cards_in_deck = Game::NumCardsInDeck();
  // The number of possible hole card pairs containing a given card
  int num_buddies = (num_cards_in_deck - num_board_cards) - 1;
  int *seen = new int[max_card + 1];
  for (int i = 0; i <= max_card; ++i) seen[i] = 0;
  int *beats = new int[num_hole_card_pairs];
  int last_hv = -1;
  int j = 0;
  while (j < num_hole_card_pairs) {
    int begin_range = j;
    // Make three passes through the range of equally strong hands
    // First pass computes win counts for each hand and finds end of range
    // Second pass updates cumulative counters
    // Third pass computes lose counts for each hand
    while (j < num_hole_card_pairs) {
      int hv = v[j].first;
      int enc = v[j].second;
      int hi = enc / (max_card + 1);
      int lo = enc % (max_card + 1);
      if (hv != last_hv) {
        last_hv = hv;
        if (j != 0) break;
      }
      beats[j] = begin_range - seen[hi] - seen[lo];
      ++j;
    }
    // Positions begin_range...j-1 (inclusive) all have the same hand value
    for (int k = begin_range; k < j; ++k) {
      int enc = v[k].second;
      int hi = enc / (max_card + 1);
      int lo = enc % (max_card + 1);
      ++seen[hi];
      ++seen[lo];
    }
    if (wins) {
      for (int k = begin_range; k < j; ++k) {
        int enc = v[k].second;
        values[enc] = (short)beats[k];
      }
    } else {
      short base_lose = num_hole_card_pairs - j;
      for (int k = begin_range; k < j; ++k) {
        int enc = v[k].second;
        int hi = enc / (max_card + 1);
        int lo = enc % (max_card + 1);
        // With five-card boards, there should be 46 hole card pairs containing,
        // say, Kc.  52 cards - 5 on board - Kc
        short lose =
            base_lose - ((num_buddies - seen[hi]) + (num_buddies - seen[lo]));
        // beats[k] and lose are the two values we care about
        // beats[k] - lose is the WML
        short wml = ((short)beats[k]) - lose;
        values[enc] = wml;
        // OutputTwoCards(hi, lo);
        // printf(" %i %i %i %u\n", (int)wml, beats[k], (int)lose, enc);
      }
    }
  }
  delete[] seen;
  delete[] beats;
  return values;
}

static float *OppoClusterRiverHandStrength(const Card *board, bool wins,
                                           const Buckets *bucket) {
  int max_street = Game::MaxStreet();
  int num_board_cards = Game::NumBoardCards(max_street);

  vector<int> vb(num_board_cards);
  for (int i = 0; i < num_board_cards; ++i) vb[i] = board[i];
  std::sort(vb.begin(), vb.end());
  int *sorted_board = new int[num_board_cards];
  for (int i = 0; i < num_board_cards; ++i) {
    sorted_board[(num_board_cards - 1) - i] = vb[i];
  }
  int num_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  int max_card = Game::MaxCard();
  int num_enc = (max_card + 1) * (max_card + 1);
  vector<int> v(num_hole_card_pairs);
  for (int i = 0; i < num_hole_card_pairs; ++i) v[i] = -1;
  vector<vector<std::pair<int, int>>> ocv(bucket->NumBuckets(0));
  int hole_cards[2];
  int hcp = 0;
  int h = 0;
  for (int hi = 1; hi <= max_card; ++hi) {
    hole_cards[0] = hi;
    for (int lo = 0; lo < hi; ++lo) {
      if (InCards(hi, board, num_board_cards) ||
          InCards(lo, board, num_board_cards)) {
        ++h;
        continue;
      }
      hole_cards[1] = lo;
      int hv = HandValueTree::Val(sorted_board, hole_cards);
      int enc = hi * (max_card + 1) + lo;
      v[hcp] = hv;
      int b = bucket->Bucket(0, h);
      if (b > ocv.size()) {
        fprintf(stderr, "bucket index overflow.");
      }
      ocv[b].push_back({enc, hv});
      ++hcp;
      ++h;
    }
  }
  delete[] sorted_board;
  float *values = new float[num_hole_card_pairs * ocv.size()];
  hcp = 0;
  for (int hi = 1; hi <= max_card; ++hi) {
    for (int lo = 0; lo < hi; ++lo) {
      if (InCards(hi, board, num_board_cards) ||
          InCards(lo, board, num_board_cards)) {
        continue;
      }
      float *target_v = values + hcp * ocv.size();
      int current_v = v[hcp];
      for (int oci = 0; oci != ocv.size(); ++oci) {
        target_v[oci] = 0;
        int valid_opp = 0;
        for (int ochi = 0; ochi != ocv[oci].size(); ++ochi) {
          int opp_enc = ocv[oci][ochi].first;
          int opp_v = ocv[oci][ochi].second;
          int opp_hi = opp_enc / (max_card + 1);
          int opp_lo = opp_enc % (max_card + 1);
          if (hi == opp_hi || hi == opp_lo || lo == opp_hi || lo == opp_lo) {
            continue;
          }
          target_v[oci] += current_v > opp_v;
          target_v[oci] += 0.5 * (current_v == opp_v);
          valid_opp += 1;
        }
        target_v[oci] /= valid_opp;
      }
      ++hcp;
    }
  }
  return values;
}

static vector<int> *NextRoundPotential(Card *board, const Buckets *bucket,
                                       unsigned int st) {
  size_t max_card = Game::MaxCard();
  size_t num_enc = (max_card + 1) * (max_card + 1);
  vector<int> *transto = new vector<int>[num_enc];
  size_t num_board_cards = Game::NumBoardCards(st);
  size_t nst = st + 1;
  size_t num_new_board_cards = Game::NumCardsForStreet(nst);
  int num_hole_card_pairs = Game::NumHoleCardPairs(nst);
  if (num_new_board_cards == 1) {
    size_t h = 0;
    for (size_t c = 0; c <= max_card; ++c) {
      if (InCards(c, board, num_board_cards)) continue;
      board[num_board_cards] = c;
      for (int hi = 1; hi <= max_card; ++hi) {
        for (int lo = 0; lo < hi; ++lo) {
          if (InCards(hi, board, num_board_cards) ||
              InCards(lo, board, num_board_cards)) {
            continue;
          }
          int b = bucket->Bucket(nst, h);
          size_t enc = hi * (max_card + 1) + lo;
          transto[enc].push_back(b);
          ++h;
        }
      }
    }
  } else if (num_new_board_cards == 3) {
    size_t h = 0;
    size_t hi, mid, lo;
    for (hi = 2; hi <= max_card; ++hi) {
      fprintf(stderr, "hi %u/%u\n", hi, max_card + 1);
      if (InCards(hi, board, num_board_cards)) continue;
      board[num_board_cards] = hi;
      for (mid = 1; mid < hi; ++mid) {
        if (InCards(mid, board, num_board_cards)) continue;
        board[num_board_cards + 1] = mid;
        for (lo = 0; lo < mid; ++lo) {
          board[num_board_cards + 2] = lo;
          for (int hi = 1; hi <= max_card; ++hi) {
            for (int lo = 0; lo < hi; ++lo) {
              if (InCards(hi, board, num_board_cards) ||
                  InCards(lo, board, num_board_cards)) {
                continue;
              }
              int b = bucket->Bucket(nst, h);
              size_t enc = hi * (max_card + 1) + lo;
              transto[enc].push_back(b);
              ++h;
            }
          }
        }
      }
    }
  } else {
    fprintf(stderr, "Unhandled number of new board cards: %u\n",
            num_new_board_cards);
    exit(-1);
  }
  return transto;
}

static void GetPercentiles(vector<short> &wmls, double *percentiles,
                           unsigned int num_percentiles,
                           short *my_percentiles) {
  sort(wmls.begin(), wmls.end());
  int num = wmls.size();
  for (int i = 0; i < num_percentiles; ++i) {
    double percentile = percentiles[i];
    int j = percentile * (num - 1) + 0.5;
    if (j >= num) {
      fprintf(stderr, "OOB pct %f j %u num %u\n", percentile, j, num);
      exit(-1);
    }
    short wml = wmls[j];
    my_percentiles[i] = wml;
  }
}

static void GetPercentiles(vector<int> &features, double *percentiles,
                           unsigned int num_percentiles, int *my_percentiles) {
  sort(features.begin(), features.end());
  int num = features.size();
  for (int i = 0; i < num_percentiles; ++i) {
    double percentile = percentiles[i];
    int j = percentile * (num - 1) + 0.5;
    if (j >= num) {
      fprintf(stderr, "OOB pct %f j %u num %u\n", percentile, j, num);
      exit(-1);
    }
    int feat = features[j];
    my_percentiles[i] = feat;
  }
}

// We need to pool the WMLs for all the variants of each canonical hand.
// What about for the flop/turn/river?
static short *ComputePreflopPercentiles(double *percentiles,
                                        unsigned int num_percentiles,
                                        bool wins) {
  BoardTree::BuildBoardCounts();
  int max_street = Game::MaxStreet();
  int num_ms_board_cards = Game::NumBoardCards(max_street);
  // Num cards left after board cards and hole cards for target player
  // removed from deck.
  int num_remaining =
      Game::NumCardsInDeck() - num_ms_board_cards - Game::NumCardsForStreet(0);
  // Assume two hole cards
  int max_wml = num_remaining * (num_remaining - 1) / 2;
  int num_wmls = 2 * max_wml + 1;
  int max_card = Game::MaxCard();
  int num_enc = (max_card + 1) * (max_card + 1);
  int **wml_counts = new int *[num_enc];
  for (int enc = 0; enc < num_enc; ++enc) {
    wml_counts[enc] = new int[num_wmls];
    for (int w = 0; w < num_wmls; ++w) wml_counts[enc][w] = 0;
  }
  int num_boards = BoardTree::NumBoards(max_street);
  for (int bd = 0; bd < num_boards; ++bd) {
    if (bd % 1000 == 0) fprintf(stderr, "bd %i/%i\n", bd, num_boards);
    const Card *board = BoardTree::Board(max_street, bd);
    int board_count = BoardTree::BoardCount(max_street, bd);
    short *river_wmls = RiverHandStrength(board, wins);
    for (int enc = 0; enc < num_enc; ++enc) {
      short wml = river_wmls[enc];
      if (wml != 32000) {
        // The raw WML values can be negative.  They range from -990 to 990,
        // I think, for full-deck holdem.  We normalize them to the range
        // 0 to 1980.
        int norm_wml = wml + max_wml;
        wml_counts[enc][norm_wml] += board_count;
      }
    }
    delete[] river_wmls;
  }
  CanonicalCards preflop_hands(2, NULL, 0, 0, false);
  int num_hole_card_pairs = Game::NumHoleCardPairs(0);
  int num_vals = num_hole_card_pairs * num_percentiles;
  short *pct_vals = new short[num_vals];
  // Take all the counts for non-canonical hands and add them to the
  // counts for the canonical hands.
  int hcp = 0;
  for (int hi = 1; hi <= max_card; ++hi) {
    for (int lo = 0; lo < hi; ++lo) {
      int enc = hi * (max_card + 1) + lo;
      if (preflop_hands.NumVariants(hcp) == 0) {
        int canon_enc = preflop_hands.Canon(hcp);
        for (int w = 0; w < num_wmls; ++w) {
          wml_counts[canon_enc][w] += wml_counts[enc][w];
          wml_counts[enc][w] = 0;
        }
      }
      ++hcp;
    }
  }

  hcp = 0;
  int *canon_enc_to_hcp = new int[num_enc];
  for (int hi = 1; hi <= max_card; ++hi) {
    for (int lo = 0; lo < hi; ++lo) {
      if (preflop_hands.NumVariants(hcp) > 0) {
        int enc = hi * (max_card + 1) + lo;
        canon_enc_to_hcp[enc] = hcp;
        int *counts = wml_counts[enc];
        int sum_counts = 0;
        for (int w = 0; w < num_wmls; ++w) sum_counts += counts[w];
        int p = 0, cum = 0;
        int w = 0;
        while (p < num_percentiles && w < num_wmls) {
          double pct = percentiles[p];
          int threshold = pct * sum_counts;
          cum += counts[w];
          if (cum >= threshold) {
            // Undo the normalization
            pct_vals[hcp * num_percentiles + p] = (short)(w - max_wml);
            OutputTwoCards(hi, lo);
            printf(" %f %i\n", pct, w - max_wml);
            fflush(stdout);
            ++p;
          }
          ++w;
        }
        // Should get to the last percentile before the end of the WMLs
        if (p < num_percentiles) {
          fprintf(stderr, "p %u expected %u\n", p, num_percentiles);
        }
      }
      ++hcp;
    }
  }

  for (int enc = 0; enc < num_enc; ++enc) {
    delete[] wml_counts[enc];
  }
  delete[] wml_counts;

  // Copy the percentile values from the canonical hands to the
  // non-canonical ones.
  hcp = 0;
  for (int hi = 1; hi <= max_card; ++hi) {
    for (int lo = 0; lo < hi; ++lo) {
      if (preflop_hands.NumVariants(hcp) == 0) {
        int canon_hcp = canon_enc_to_hcp[preflop_hands.Canon(hcp)];
        for (int p = 0; p < num_percentiles; ++p) {
          pct_vals[hcp * num_percentiles + p] =
              pct_vals[canon_hcp * num_percentiles + p];
        }
      }
      ++hcp;
    }
  }

  delete[] canon_enc_to_hcp;

  return pct_vals;
}

// Not practical for preflop for full-deck holdem.
static vector<short> *ComputeRollout(Card *board, bool wins, unsigned int st) {
  size_t max_street = Game::MaxStreet();
  size_t max_card = Game::MaxCard();
  size_t num_enc = (max_card + 1) * (max_card + 1);
  vector<short> *wmls = new vector<short>[num_enc];
  if (st == max_street) {
    short *wmls1 = RiverHandStrength(board, wins);
    for (size_t i = 0; i < num_enc; ++i) {
      short wml = wmls1[i];
      if (wml != 32000) {
        wmls[i].push_back(wml);
      }
    }
    delete[] wmls1;
  } else {
    size_t num_board_cards = Game::NumBoardCards(st);
    size_t nst = st + 1;
    size_t num_new_board_cards = Game::NumCardsForStreet(nst);
    if (num_new_board_cards == 1) {
      for (size_t c = 0; c <= max_card; ++c) {
        if (InCards(c, board, num_board_cards)) continue;
        board[num_board_cards] = c;
        vector<short> *next_wmls = ComputeRollout(board, wins, nst);
        for (size_t i = 0; i < num_enc; ++i) {
          vector<short> &v = next_wmls[i];
          size_t num = v.size();
          for (size_t j = 0; j < num; ++j) {
            wmls[i].push_back(v[j]);
          }
        }
        delete[] next_wmls;
      }
    } else if (num_new_board_cards == 3) {
      size_t hi, mid, lo;
      for (hi = 2; hi <= max_card; ++hi) {
        fprintf(stderr, "hi %u/%u\n", hi, max_card + 1);
        if (InCards(hi, board, num_board_cards)) continue;
        board[num_board_cards] = hi;
        for (mid = 1; mid < hi; ++mid) {
          if (InCards(mid, board, num_board_cards)) continue;
          board[num_board_cards + 1] = mid;
          for (lo = 0; lo < mid; ++lo) {
            board[num_board_cards + 2] = lo;
            vector<short> *next_wmls = ComputeRollout(board, wins, nst);
            for (size_t i = 0; i < num_enc; ++i) {
              vector<short> &v = next_wmls[i];
              size_t num = v.size();
              for (size_t j = 0; j < num; ++j) {
                wmls[i].push_back(v[j]);
              }
            }
            delete[] next_wmls;
          }
        }
      }
    } else {
      fprintf(stderr, "Unhandled number of new board cards: %u\n",
              num_new_board_cards);
      exit(-1);
    }
  }
  return wmls;
}

// On river.  Compute WML for all hands.
float *OppoClusterComputeRollout(unsigned int st, bool wins,
                                 const Buckets *oppo_bucket, int nb_threads) {
  if (st != Game::MaxStreet()) {
    fprintf(stderr, "Oppo Cluster can only be used for last street.");
    return nullptr;
  }
  size_t num_boards = BoardTree::NumBoards(st);
  size_t num_board_cards = Game::NumBoardCards(st);
  size_t num_hole_card_pairs = Game::NumHoleCardPairs(st);
  size_t num_hands = num_boards * num_hole_card_pairs;
  size_t num_features = oppo_bucket->NumBuckets(0);
  size_t num_vals = num_hands * num_features;
  float *pct_vals = new float[num_vals];
  int per_block = num_boards / nb_threads;
  std::vector<size_t> thread_block_size(nb_threads, per_block);
  thread_block_size.back() += num_boards - nb_threads * per_block;
  std::vector<std::thread> eval_threads(nb_threads);
  size_t accumulator = 0;
  for (int t = 0; t < nb_threads; ++t) {
    accumulator += thread_block_size[t];
    eval_threads[t] = std::thread([t, accumulator, &thread_block_size, st, wins,
                                   oppo_bucket, num_boards, num_board_cards,
                                   num_hole_card_pairs, num_hands, num_features,
                                   num_vals, pct_vals] {
      Card board[5];
      for (size_t bd = (accumulator - thread_block_size[t]); bd < accumulator;
           ++bd) {
        fprintf(stderr, "bd %u/%u\n", bd, num_boards);
        const Card *st_board = BoardTree::Board(st, bd);
        for (size_t i = 0; i < num_board_cards; ++i) {
          board[i] = st_board[i];
        }
        size_t h = bd * num_hole_card_pairs;
        float *value = OppoClusterRiverHandStrength(board, wins, oppo_bucket);
        for (int i = 0; i != num_hole_card_pairs * num_features; ++i) {
          pct_vals[h * num_features + i] = value[i];
        }
        delete[] value;
      }
    });
  }
  for (int t = 0; t < nb_threads; ++t) eval_threads[t].join();
  return pct_vals;
}

// On turn, deal out all river cards.  Compute WML for all hands.
int *ComputePotentialRollout(unsigned int st, double *percentiles,
                             unsigned int num_percentiles,
                             Buckets *next_bucket) {
  size_t num_boards = BoardTree::NumBoards(st);
  size_t num_board_cards = Game::NumBoardCards(st);
  size_t num_hole_card_pairs = Game::NumHoleCardPairs(st);
  size_t num_hands = num_boards * num_hole_card_pairs;
  size_t num_vals = num_hands * num_percentiles;
  int *pct_vals;
  if (st > 0) {
    pct_vals = new int[num_vals];
    Card board[5];
    for (size_t bd = 0; bd < num_boards; ++bd) {
      fprintf(stderr, "bd %u/%u\n", bd, num_boards);
      const Card *st_board = BoardTree::Board(st, bd);
      for (size_t i = 0; i < num_board_cards; ++i) {
        board[i] = st_board[i];
      }
      size_t h = bd * num_hole_card_pairs;
      vector<int> *features = NextRoundPotential(board, next_bucket, st);
      size_t max_card = Game::MaxCard();
      size_t num_enc = (max_card + 1) * (max_card + 1);
      for (size_t i = 0; i < num_enc; ++i) {
        vector<int> &v = features[i];
        if (v.size() == 0) continue;
        GetPercentiles(v, percentiles, num_percentiles,
                       &pct_vals[h * num_percentiles]);
        ++h;
      }
      delete[] features;
    }
  }
  return pct_vals;
}

// On turn, deal out all river cards.  Compute WML for all hands.
short *ComputeRollout(unsigned int st, double *percentiles,
                      unsigned int num_percentiles, double squashing,
                      bool wins) {
  size_t num_boards = BoardTree::NumBoards(st);
  size_t num_board_cards = Game::NumBoardCards(st);
  size_t num_hole_card_pairs = Game::NumHoleCardPairs(st);
  size_t num_hands = num_boards * num_hole_card_pairs;
  size_t num_vals = num_hands * num_percentiles;
  short *pct_vals;
  if (st == 0) {
    pct_vals = ComputePreflopPercentiles(percentiles, num_percentiles, wins);
  } else {
    pct_vals = new short[num_vals];
    Card board[5];
    for (size_t bd = 0; bd < num_boards; ++bd) {
      fprintf(stderr, "bd %u/%u\n", bd, num_boards);
      const Card *st_board = BoardTree::Board(st, bd);
      for (size_t i = 0; i < num_board_cards; ++i) {
        board[i] = st_board[i];
      }
      size_t h = bd * num_hole_card_pairs;
      vector<short> *wmls = ComputeRollout(board, wins, st);
      size_t max_card = Game::MaxCard();
      size_t num_enc = (max_card + 1) * (max_card + 1);
      for (size_t i = 0; i < num_enc; ++i) {
        vector<short> &v = wmls[i];
        if (v.size() == 0) continue;
        GetPercentiles(v, percentiles, num_percentiles,
                       &pct_vals[h * num_percentiles]);
        ++h;
      }
      delete[] wmls;
    }
  }
  if (squashing == 1.0) {
    return pct_vals;
  }
  short min_val = 32700;
  short max_val = -32700;
  for (size_t i = 0; i < num_vals; ++i) {
    short val = pct_vals[i];
    if (val < min_val) min_val = val;
    if (val > max_val) max_val = val;
  }
  // Renormalize vals so that worse hands have higher values and the lowest
  // value is zero.  Then squash by raising to the given power.
  for (size_t i = 0; i < num_vals; ++i) {
    short val = pct_vals[i];
    short norm_val = -(val - max_val);
    pct_vals[i] = (short)pow(norm_val, squashing);
  }
  return pct_vals;
}
