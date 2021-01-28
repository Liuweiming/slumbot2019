#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "board_tree.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"

using std::unique_ptr;

using std::pair;
using std::vector;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params>\n", prog_name);
  exit(-1);
}

std::string cards_to_string(const std::vector<Card> &cards) {
  std::string out;
  for (auto &c : cards) {
    std::string cs;
    CardName(c, &cs);
    out += cs[0];
  }
  return out;
}

std::string cards_to_string(Card *cards, int n) {
  std::string out;
  for (int i = 0; i != n; ++i) {
    std::string c;
    CardName(cards[i], &c);
    out += c[0];
  }
  return out;
}

int main(int argc, char *argv[]) {
  if (argc != 3) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> card_params = CreateCardAbstractionParams();
  card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction> card_abstraction(
      new CardAbstraction(*card_params));
  Buckets buckets(*card_abstraction, false);
  int max_street = Game::MaxStreet();

  for (int st = 0; st <= max_street; ++st) {
    // Just need this to get number of hands
    BoardTree::Create();
    unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    unsigned int num_board_cards = (unsigned int)BoardTree::NumBoards(st);
    Card board[5];
    Card hole_cards[2];
    fprintf(stdout, "street %i has %i buckets\n", st, buckets.NumBuckets(st));
    if (!buckets.NumBuckets(st)) {
      continue;
    }

    std::unordered_map<int, std::unordered_set<std::string>> bucket_index;
    unsigned bucket_map[13][13];
    unsigned int max_card = Game::MaxCard();

    if (st && num_board_cards) {
      for (unsigned int bd = 0; bd < num_board_cards; ++bd) {
        const Card *st_board = BoardTree::Board(st, bd);
        for (unsigned int i = 0; i < num_board_cards; ++i) {
          board[i] = st_board[i];
        }
        unsigned int hcp = 0;
        for (int hi = 1; hi <= max_card; ++hi) {
          if (InCards(hi, board, num_board_cards)) continue;
          hole_cards[0] = hi;
          for (int lo = 0; lo < hi; ++lo) {
            if (InCards(lo, board, num_board_cards)) continue;
            hole_cards[1] = lo;
            unsigned int h = bd * num_hole_card_pairs + hcp;
            std::string cards_str = cards_to_string(hole_cards, 2);
            cards_str += "/";
            cards_str += cards_to_string(board, num_board_cards);
            bucket_index[buckets.Bucket(st, h)].insert(cards_str);
          }
        }
      }
    } else {
      unsigned int hcp = 0;
      for (int hi = 1; hi <= max_card; ++hi) {
        hole_cards[0] = hi;
        for (int lo = 0; lo < hi; ++lo) {
          hole_cards[1] = lo;
          unsigned int h = hcp;
          std::string cards_str = cards_to_string(hole_cards, 2);
          bucket_index[buckets.Bucket(st, h)].insert(cards_str);
          if (Suit(hi) == Suit(lo)) {
            bucket_map[Rank(lo)][Rank(hi)] = buckets.Bucket(st, h);
          } else {
            bucket_map[Rank(hi)][Rank(lo)] = buckets.Bucket(st, h);
          }
          ++hcp;
        }
      }
    }
    for (unsigned int b = 0; b != bucket_index.size(); ++b) {
      auto &cards_in_bucket = bucket_index[b];
      fprintf(stdout, "\n%d: ", b);
      for (auto &cs : cards_in_bucket) {
        fprintf(stdout, "%s ", cs.c_str());
      }
    }
    if (!st) {
      std::string colormap[13] = {
          "\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m",
          "\033[36m", "\033[37m", "\033[91m", "\033[92m", "\033[93m",
          "\033[94m", "\033[95m", "\033[96m",
      };
      fprintf(stdout, "\n");
      for (int i = 0; i != 13; ++i) {
        for (int j = 0; j != 13; ++j) {
          fprintf(stdout, colormap[bucket_map[i][j]].c_str());
          fprintf(stdout, "%4d", bucket_map[i][j]);
          fprintf(stdout, "\033[0m");
        }
        fprintf(stdout, "\n");
      }
    }
  }
}
