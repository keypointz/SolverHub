#include "mesh_point.h"

// 实现 get_point_by_id 方法
mesh_Point* mesh_PointList::get_point_by_id(int id) {
    mesh_Point* current = head_point;
    while (current) {
        if (current->id == id) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}
