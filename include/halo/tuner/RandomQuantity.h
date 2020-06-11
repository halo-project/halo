#pragma once

#include <gsl/gsl_vector.h>
#include <gsl/gsl_statistics_double.h>
#include <cmath>
#include <memory>
#include <cassert>

namespace halo {

// A simple wrapper for gsl_vector to manage its memory, and also some user-friendly additions.
class GSLVector {
public:
  GSLVector(size_t Sz, bool Zeroed=true)
    : Vec(gsl_vector_alloc(Sz), gsl_vector_free) {
      if (Zeroed)
        gsl_vector_set_all(Vec.get(), 0);
    }

  // number of elements in the vector
  size_t size() const { return Vec->size; }

  // The stride is the step-size from one element to the next in physical memory,
  // measured in units of the appropriate datatype.
  size_t stride() const { return Vec->stride; }

  // access underlying vector, without taking ownership.
  gsl_vector* vec() { return Vec.get(); }
  gsl_vector const* vec() const { return Vec.get(); }

  // access the underlying vector's data
  double* data() { return Vec->data; }
  double const* data() const {return Vec->data; }

  // read an element from the vector
  double get(size_t idx) const { return gsl_vector_get(Vec.get(), idx); }

  // write an element of the vector
  void set(size_t idx, double val) { gsl_vector_set(Vec.get(), idx, val); }

private:
  // a managed gsl_vector, which must be constructed with an explicit deleter.
  std::unique_ptr<gsl_vector, decltype(&gsl_vector_free)> Vec;
};


/// A value whose quantity depends on some random phenomenon.
/// Not all observations are used to compute properties of the quantity,
/// with priority given to the most recent N observations, for some N.
class RandomQuantity {
public:
  RandomQuantity(size_t N=10) : Obs(N) {}

  /// Records an observation of the random quantity.
  /// If the history becomes full, the oldest observation is dropped to make room
  /// for this one.
  void observe(double NewSample) {
    Count += 1;
    Obs.set(NextFree, NewSample);

    const size_t Sz = Obs.size();
    NextFree = (NextFree + 1) % Sz; // bump with wrap-around

    // If this is the first observation, fill the vector with this value
    if (Count == 1)
      for (size_t i = NextFree; i < Sz; i++)
        Obs.set(i, NewSample);
  }

  /// returns the number of individual observations currently remembered
  /// by this random quantity.
  auto observations() const { return std::min(Count, Obs.size()); }

  double mean() const {
    return gsl_stats_mean(Obs.data(), Obs.stride(), observations());
  }

  private:
    GSLVector Obs;
    size_t NextFree{0};
    size_t Count{0};
};

} // end namespace halo