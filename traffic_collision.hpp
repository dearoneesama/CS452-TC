#pragma once

#include "traffic_mini_driver.hpp"

namespace traffic {
  struct collision_avoider {
    etl::vector<internal_train_state *, tracks::num_trains> *initialized_trains;  // observer
    etl::unordered_map<int, mini_driver, tracks::num_trains> *drivers;  // observer
    tracks::switch_status_t *switches;  // observer

  private:
    /**
     * track segments (sensors) locked by trains.
     */
    etl::unordered_map<int, tracks::node_path_segment_vec_t, tracks::num_trains> train_node_locks {};
    /**
     * want to avoid sending multiple clearances to same train.
     */
    etl::unordered_set<int, tracks::num_trains> cleared_trains {};

  public:
    collision_avoider(
      decltype(initialized_trains) initialized_trains_,
      decltype(drivers) drivers_,
      decltype(switches) switches_
    ) : initialized_trains(initialized_trains_), drivers(drivers_), switches(switches_) {
      for (auto &[num, driver]: *drivers) {
        cleared_trains.insert(num);
      }
    }

  private:
    void get_new_sensor_locks();
    void send_sensor_locks() const;

    void handle_train(mini_driver &driver);

  public:
    /**
     * check for potential collisions for all initialized trains/drivers, and
     * give instructions to drivers to avoid them.
     */
    void perform();

    void remove_train(int num) {
      train_node_locks.erase(num);
    }
  };

}  // namespace traffic
