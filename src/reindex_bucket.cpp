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
extern "C" {
#include "hand_index-impl.h"
#include "hand_index.h"
}
#include "io.h"
#include "params.h"

using std::unique_ptr;

using std::pair;
using std::vector;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <bucketing>\n",
          prog_name);
  exit(-1);
}

std::string cards_to_string(const std::vector<Card> &cards) {
  std::string out;
  for (auto &c : cards) {
    std::string cs;
    CardName(c, &cs);
    out += cs;
  }
  return out;
}

std::string cards_to_string(Card *cards, int n) {
  std::string out;
  for (int i = 0; i != n; ++i) {
    std::string c;
    CardName(cards[i], &c);
    out += c;
  }
  return out;
}

static void Write(int st, const std::string &bucketing,
                  unsigned int *assigments, int num_hands, int num_buckets) {
  int max_street = Game::MaxStreet();
  char buf[500];
  sprintf(buf, "%s/buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(),
          Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits(),
          max_street, bucketing.c_str(), st);
  Writer writer(buf);
  writer.WriteInt(st);
  writer.WriteInt(num_buckets);
  for (unsigned int h = 0; h < num_hands; ++h) {
    int b = assigments[h];
    if (b == -1){
      fprintf(stderr, "unassigned hand %u", h);
      exit(-1);
    }
    writer.WriteUnsignedInt(b);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 4) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> card_params = CreateCardAbstractionParams();
  card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction> card_abstraction(
      new CardAbstraction(*card_params));
  Buckets buckets(*card_abstraction, false);
  std::string bucketing = argv[3];
  int max_street = Game::MaxStreet();

  uint8_t num_cards1[1] = {2};
  hand_indexer_t indexer[4];
  hand_indexer_init(1, num_cards1, &indexer[0]);
  uint8_t num_cards2[2] = {2, 3};
  hand_indexer_init(2, num_cards2, &indexer[1]);
  uint8_t num_cards3[2] = {2, 4};
  hand_indexer_init(2, num_cards3, &indexer[2]);
  uint8_t num_cards4[2] = {2, 5};
  hand_indexer_init(2, num_cards4, &indexer[3]);

  for (int st = 0; st <= max_street; ++st) {
    // Just need this to get number of hands
    BoardTree::Create();
    size_t num_hole_card_pairs = Game::NumHoleCardPairs(st);
    size_t num_board_cards = Game::NumBoardCards(st);
    size_t num_boards = (unsigned int)BoardTree::NumBoards(st);
    if (st == 0) {
      num_boards = 1;
    }
    size_t num_hands = num_boards * num_hole_card_pairs;
    size_t round_size = indexer[st].round_size[(st == 0) ? 0 : 1];
    size_t num_buckets = buckets.NumBuckets(st);
    Card board[5];
    Card hole_cards[2];
    fprintf(stdout, "street %i has %i buckets\n", st, buckets.NumBuckets(st));
    fprintf(stdout,
            "hand %lu, round size %lu, hole_pairs %lu, num_boards %lu\n",
            num_hands, round_size, num_hole_card_pairs, num_boards);

    if (!buckets.NumBuckets(st)) {
      continue;
    }

    unsigned int max_card = Game::MaxCard();
    unsigned int *assigments = new unsigned int[round_size];
    for (unsigned int ai = 0; ai != round_size; ++ai) {
      assigments[ai] = -1;
    }
    uint8_t cards[7];
    for (unsigned int bd = 0; bd < num_boards; ++bd) {
      if (st) {
        const Card *st_board = BoardTree::Board(st, bd);
        for (unsigned int i = 0; i < num_board_cards; ++i) {
          board[i] = st_board[i];
          cards[i + 2] = st_board[i];
        }
      }
      unsigned int hcp = 0;
      for (int hi = 1; hi <= max_card; ++hi) {
        if (st && InCards(hi, board, num_board_cards)) continue;
        hole_cards[0] = hi;
        for (int lo = 0; lo < hi; ++lo) {
          if (st && InCards(lo, board, num_board_cards)) continue;
          hole_cards[1] = lo;
          cards[0] = hole_cards[0];
          cards[1] = hole_cards[1];
          unsigned int h = bd * num_hole_card_pairs + hcp;
          unsigned int b = buckets.Bucket(st, h);
          // std::string cards_str = cards_to_string(hole_cards, 2);
          // cards_str += "/";
          // cards_str += cards_to_string(board, num_board_cards);
          // fprintf(stdout, "%s: hand %u, bucket %u\n",
          //         cards_str.c_str(), h, b);
          // std::flush(std::cout);
          // exit(0);
          hand_index_t index = hand_index_last(&indexer[st], cards);
          if (h % 100000 == 0) {
            fprintf(stdout, "hand %u, index %lu, bucket %u\n", h, index, b);
          }
          if (index > round_size) {
            fprintf(stderr, "index overflow.\n");
          }
          if (assigments[index] != -1 && assigments[index] != b) {
            fprintf(stderr, "inconsistent bucketing.\n");
            std::string cards_str = cards_to_string(hole_cards, 2);
            cards_str += "/";
            cards_str += cards_to_string(board, num_board_cards);
            fprintf(stdout, "%s: hand %u, index %lu, bucket %u\n",
                    cards_str.c_str(), h, index, b);
            exit(1);
          }
          assigments[index] = b;
          ++hcp;
        }
      }
    }
    Write(st, bucketing, assigments, round_size, num_buckets);
    delete[] assigments;
  }
}
