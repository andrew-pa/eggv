#pragma once
#include "cmmn.h"
#include <unordered_map>
#include <utility>

using entity_id = size_t;
using system_id = size_t;

enum class static_systems : system_id {
    transform, light, camera
};

#include "scene_graph.h"

// struct frame_state {
//     float t, dt;
//     std::map<std::string, bool>* gui_open_windows;
//
//     frame_state(float t, float dt, std::map<std::string, bool>* gow)
//         : t(t), dt(dt), gui_open_windows(gow) {}
// };

class abstract_entity_system {
public:
    virtual void update(const frame_state& fs, class world* w) {}
    virtual void remove_entity(entity_id id) = 0;

    virtual void build_gui(const frame_state& fs, entity_id selected_entity) = 0;

    virtual ~abstract_entity_system() = default;
};

struct unordered_map_storage {
    template<typename T>
    using type = std::unordered_map<entity_id, T>;

    template<typename T>
    static void emplace(type<T>& self, entity_id id, T data) {
        self.emplace(id, data);
    }

    template<typename T>
    static void remove(type<T>& self, entity_id id) {
        self.erase(id);
    }

    template<typename T>
    static bool contains(type<T>& self, entity_id id) {
        return self.contains(id);
    }

    template<typename T>
    static T& get(type<T>& self, entity_id id) {
        auto i = self.find(id);
        if(i == self.end()) throw "not found";
        return i->second;
    }

    template<typename T>
    static const T& get(const type<T>& self, entity_id id) {
        auto i = self.find(id);
        if(i == self.end()) throw "not found";
        return i->second;
    }
};

struct assoc_vector_storage {
    template<typename T>
    using type = std::vector<std::pair<entity_id, T>>;

    template<typename T>
    static void emplace(type<T>& self, entity_id id, T data) {
        self.emplace_back(id, data);
    }

    template<typename T>
    static void remove(type<T>& self, entity_id id) {
        auto i = std::find_if(self.begin(), self.end(), [id](const auto& p) { return p.first == id; });
        self.remove(i);
    }

    template<typename T>
    static bool contains(type<T>& self, entity_id id) {
        auto i = std::find_if(self.begin(), self.end(), [id](const auto& p) { return p.first == id; });
        return i != self.end();
    }

    template<typename T>
    static T& get(type<T>& self, entity_id id) {
        return std::any_of(self.begin(), self.end(), [id](const auto& p) { return p.first == id; });
    }

    template<typename T>
    static const T& get(const type<T>& self, entity_id id) {
        auto i = std::find_if(self.begin(), self.end(), [id](const auto& p) { return p.first == id; });
        if(i == self.end()) throw "not found";
        return i->second;
    }
};

template<typename Component, typename Storage = unordered_map_storage>
class entity_system : public abstract_entity_system {
protected:
    typename Storage::template type<Component> entity_data;
public:
    using component_t = Component;
    using storage_t = Storage;

    virtual void add_entity(entity_id id, Component data) {
        Storage::template emplace<Component>(this->entity_data, id, data);
    }

    void remove_entity(entity_id id) override {
        Storage::template remove<Component>(this->entity_data, id);
    }

    bool has_data_for_entity(entity_id id) const {
        return Storage::template contains<Component>(this->entity_data, id);
    }

    const Component& get_data_for_entity(entity_id id) const {
        return Storage::template get<Component>(this->entity_data, id);
    }

    Component& get_data_for_entity(entity_id id) {
        return Storage::template get<Component>(this->entity_data, id);
    }

    friend class world;
};

static const entity_id root_id = (entity_id)1;

class world {
    std::unordered_map<system_id, std::shared_ptr<abstract_entity_system>> systems;

    entity_id next_id;

    struct node {
        std::string name;
        entity_id entity;
        std::vector<std::shared_ptr<node>> children;

        node(entity_id id, std::string n) : name(std::move(n)), entity(id) {}
    };
    std::shared_ptr<node> root_entity;
    std::unordered_map<entity_id, std::shared_ptr<node>> nodes;
public:
    class entity_handle {
        world* w;
        std::shared_ptr<node> _node;
        entity_handle(world* w, std::shared_ptr<node> n) : w(w), _node(std::move(n)) {}
    public:
        template<typename System>
        entity_handle& add_component(
                typename System::component_t component,
                system_id id = (system_id)System::id
        ) {
            auto* system = w->system<System>(id);
            system->add_entity(_node->entity, component);
            return *this;
        }

        template<typename System>
        bool has_component(system_id id = (system_id)System::id) const {
            auto* system = w->system<System>(id);
            return system->has_data_for_entity(_node->entity);
        }

        template<typename System>
        auto get_component(system_id id = (system_id)System::id) -> typename System::component_t& {
            auto* system = w->system<System>(id);
            return system->get_data_for_entity(_node->entity);
        }

        template<typename System>
        auto get_component(system_id id = (system_id)System::id) const -> const typename System::component_t& {
            auto* system = w->system<System>(id);
            return system->get_data_for_entity(_node->entity);
        }

        void remove_component(system_id id) {
            auto system = w->systems[id];
            system->remove_entity(_node->entity);
        }

        entity_handle add_child(const std::string& name = "") {
            auto id = w->next_id++;
            auto n = std::make_shared<node>(id, name);
            _node->children.push_back(n);
            w->nodes.emplace(id, n);
            return entity_handle{w, n};
        }

        size_t num_children() const {
            return _node->children.size();
        }

        bool has_children() const {
            return !_node->children.empty();
        }

        template<typename F>
        void for_each_child(F fn) const {
            for(const auto& c : _node->children) {
                fn(entity_handle{w, c});
            }
        }

        std::string_view name() const {
            return this->_node->name;
        }

        entity_id id() const {
            return this->_node->entity;
        }

        operator entity_id() const {
            return this->_node->entity;
        }

        friend class world;
    };


    world();

    template<typename System>
    void add_system(std::shared_ptr<System> sys, system_id id = (system_id)System::id) {
        systems.emplace(id, sys);
    }

    template<typename System>
    std::shared_ptr<System> system(system_id id = (system_id)System::id) {
        return std::dynamic_pointer_cast<System>(systems.at(id));
    }

    entity_handle create_entity(const std::string& name = "") {
        std::cout << "??? " << next_id << "\n";
        auto id = next_id++;
        auto n = std::make_shared<node>(id, name);
        root_entity->children.push_back(n);
        nodes.emplace(id, n);
        return entity_handle{this, n};
    }

    entity_handle entity(entity_id id) {
        return entity_handle{this, nodes[id]};
    }

    entity_handle root() { return entity_handle{this, root_entity}; }

    void update(const frame_state& fs);
    void build_gui(const frame_state& fs);

private:
    // gui state
    entity_id selected_entity;
    void build_scene_tree_gui(const entity_handle& e);
};
