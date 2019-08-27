#pragma once

#include <algorithm>
#include <functional>

namespace halo {
  namespace util {

template <template <typename T, typename A = std::allocator<T> > class CollectionType,
          typename ItemType>
CollectionType<ItemType> drop_if(CollectionType<ItemType> &Col, std::function<bool(ItemType)> Pred) {
  auto PartitionPt = std::stable_partition(Col.begin(), Col.end(), Pred);
  CollectionType<ItemType> NewCol;
  std::move(PartitionPt, Col.end(), NewCol.begin());
  Col.erase(PartitionPt, Col.end());
  return NewCol;
}

}
}
