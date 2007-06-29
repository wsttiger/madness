/*
  This file is part of MADNESS.
  
  Copyright (C) <2007> <Oak Ridge National Laboratory>
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
  
  For more information please contact:

  Robert J. Harrison
  Oak Ridge National Laboratory
  One Bethel Valley Road
  P.O. Box 2008, MS-6367

  email: harrisonrj@ornl.gov 
  tel:   865-241-3937
  fax:   865-572-0680

  
  $Id$
*/

/// \file loadbal.h
/// \brief Declares and partially implements MyPmap, LoadBalImpl and associated load balancing classes.

  
#ifndef LOADBAL_H
#define LOADBAL_H

namespace madness {

    typedef int Cost;
    typedef double CompCost;

    /// Finds exponent k such that d^k <= me < d^{k+1}
    inline int nearest_power(int me, int d) {
        int k = 0;
        while (me != 0) {
            if (me%d == 0) {
                k++;
                me/=d;
            } else {
                break;
            }
        }
        return k;
    };

    template <typename Data, int D> class LBNode;
    template <int D> struct TreeCoords;
    template <int D> struct Tree;
    template <int D> class MyPmap;
    template <int D> class LBTree;
    class NodeData;

    /// Convenient typedef shortcuts

    /// Makes it easier to handle these unwieldy templated types
    template <int D>
    struct DClass {
        typedef Key<D> KeyD;
        typedef const Key<D> KeyDConst;
        typedef TreeCoords<D> TreeCoords;
        typedef Tree<D> Tree;
        typedef LBNode<NodeData,D> NodeD;
        typedef const LBNode<NodeData,D> NodeDConst;
        typedef MyPmap<D> MyPmap;
        typedef LBTree<D> treeT;
    };


    /// The node that is used in the fascimile copy of the tree to be load balanced

    /// The node used in the tree that is operated upon and load balanced in LoadBalImpl.
    template <typename Data, int D>
    class LBNode {
    private:
        Data data;
        std::vector<bool> c; /// Existence of each child individually

        void all_children(bool status=false) {
            c.clear();
            c.assign(dim, status);
        };

    public:
        static int dim; /// Number of children in standard tree (e.g. 2^D)

        LBNode() {
            data = Data();
            all_children();
        };

        LBNode(Data d, bool children=false) : data(d) {
            all_children(children);
        };

	/// Determines whether node has any children at all
        bool has_children() const {
            for (int i = 0; i < dim; i++)
                if (c[i]) return true;
            return false;
        };

        bool has_child(unsigned int i) const {
            return c[i];
        };

        bool has_child(int i) const {
            return c[i];
        };

        void set_child(int i, bool setto = true) {
            c[i] = setto;
        };

        void set_data(Data d) {
            data = d;
        };

        Data get_data() const {
            return data;
        };

        vector<bool> get_c() const {
            return c;
        };

        template <typename Archive>
        void serialize(const Archive& ar) {
            ar & data & c;
        }
    };


    template <typename Data, int D>
    std::ostream& operator<<(std::ostream& s, const LBNode<Data, D>& node) {
        s << "data = " << node.get_data() << ", c = " << node.get_c();
        return s;
    };

    template <int D>
    std::ostream& operator<<(std::ostream& s, typename DClass<D>::NodeDConst& node) {
        s << "data = " << node.get_data() << ", c = " << node.get_c();
        return s;
    };


    template <typename Data, int D>
    int LBNode<Data,D>::dim = power<D>();


    /// Diagnostic data contained in fascimile tree
    /// Diagnostic data, including the cost of the node and the subtree headed by that node,
    /// along with a bool flag used during depth-first partitioning
    class NodeData {
        friend std::ostream& operator<<(std::ostream& s, const NodeData& nd);
    public:
        int cost;
        int subcost;
        bool is_taken;
        NodeData(int c = 1, int s = 1, bool i = false) : cost(c), subcost(s), is_taken(i) {};
        template <typename Archive>
        void serialize(const Archive& ar) {
            ar & cost & subcost & is_taken;
        };
        void print() {
            cout << "cost = " << cost << ", subcost = " << subcost << ", is_taken = " << is_taken << endl;
        };
    };


    inline std::ostream& operator<<(std::ostream& s, const NodeData& nd) {
        s << "cost " << nd.cost << ", subcost " << nd.subcost << ", is_taken " << nd.is_taken;
        return s;
    };



    /// Key + owner, struct used to determine mapping of tree nodes
    template <int D>
    struct TreeCoords {
        Key<D> key;
        ProcessID owner;

        TreeCoords(const Key<D> k, ProcessID o) : key(Key<D>(k)), owner(o) {};
        TreeCoords(const TreeCoords& t) : key(Key<D>(t.key)), owner(t.owner) {};
        TreeCoords() : key(Key<D>()), owner(-1) {};
        void print() const {
            madness::print(key, "   owner =", owner);
        };

        bool operator< (const TreeCoords t) const {
            return (this->key < t.key);
        };
    };



    /// Tree structure in which procmap is stored
    template <int D>
    struct Tree {
        TreeCoords<D> data;
        vector<SharedPtr<Tree> > children;
        Tree* parent;

        Tree() {};
        Tree(TreeCoords<D> d) : data(d), parent(0) {};
        Tree(TreeCoords<D> d, Tree* p) : data(d), parent(p) {};

        Tree(const Tree<D>& tree) : data(tree.data), parent(0) {};
        Tree(const Tree<D>& tree, Tree* p) : data(tree.data), parent(p) {};

        Tree<D>& operator=(const Tree<D>& other) {
            if (this != &other) {
                this->data = other.data;
                this->parent = other.parent;
                this->children = other.children;
            }
            return *this;
        };

        void insert_child(TreeCoords<D> d) {
            Tree* c = new Tree(d, this);
            children.insert(children.begin(),SharedPtr<Tree<D> > (c));
        };

        void insert_child(const Tree<D>& tree) {
            Tree* c = new Tree(tree, this);
            children.insert(children.begin(),SharedPtr<Tree<D> > (c));
        };

        void print() const {
            data.print();
            int csize = children.size();
            for (int j = 0; j < csize; j++) {
                children[j]->print();
            }
        };

        bool is_foreparent_of(Key<D> key) const {
            return (this->data.key.is_parent_of(key));
        };

	/// given a key, determine the owner of the key by traversing the Tree
        void find_owner(const Key<D> key, ProcessID *ow) const {
//madness::print("find_owner: at node", this->data.key);
            if (this->is_foreparent_of(key)) {
//madness::print("find_owner: node", this->data.key, "is foreparent of", key, "so owner =", this->data.owner);
                *ow = this->data.owner;
                if (this->data.key.level() < key.level()) {
                    int csize = children.size();
                    for (int j = 0; j < csize; j++) {
//madness::print("find_owner: recursively call on ", this->children[j]->data.key);
                        children[j]->find_owner(key, ow);
                    }
                }
            }
        };

	/// Add node to the Tree in the right location
        bool fill(TreeCoords<D> node) {
            bool success = false;
            if (this->is_foreparent_of(node.key)) {
                int csize = children.size();
                for (int i = 0; i < csize; i++) {
                    if (children[i]->is_foreparent_of(node.key)) {
                        success = children[i]->fill(node);
                    }
                }
                if (!success) {
                    this->insert_child(node);
                    success = true;
                }
            }
            return success;
        }
    };


    /// Procmap implemented using Tree of TreeCoords

    template <int D>
    class MyPmap : public WorldDCPmapInterface< Key<D> > {
    private:
        bool staticmap;
        const ProcessID staticmap_owner;
        Tree<D>* tree_map;
        typedef Key<D> KeyD;

	/// private method that builds the Tree underlying the procmap
        void build_tree_map(vector<TreeCoords<D> > v) {
            sort(v.begin(), v.end());
            int vlen = v.size();

            if (vlen == 0) throw "empty map!!!";

            tree_map = new Tree<D>(v[vlen-1]);
            for (int j = vlen-2; j >= 0; j--) {
                tree_map->fill(v[j]);
            }
        };


    public:
        MyPmap() : staticmap(false), staticmap_owner(0) {};

        MyPmap(World& world) : staticmap(false), staticmap_owner(0) {
            int NP = world.nproc();
            int twotoD = power<D>();
            const int level = nearest_power(NP, twotoD);
            int NPin = (int) pow((double)twotoD,level);
            vector<TreeCoords<D> > v;

            for (Translation i=0; i < (Translation)NPin; i++) {
                KeyD key(level,i);
                if ((i%twotoD) == 0) {
                    key = key.parent(nearest_power(NPin-i, twotoD));
                }
                v.push_back(TreeCoords<D>(key,i));
            }
            build_tree_map(v);
            madness::print("MyPmap constructor");
            tree_map->print();
        };

        MyPmap(World& world, ProcessID owner) : staticmap(true), staticmap_owner(owner) {};

        MyPmap(World& world, vector<TreeCoords<D> > v) : staticmap(false), staticmap_owner(0) {
            build_tree_map(v);
            madness::print("");
            tree_map->print();
        };

        MyPmap(const MyPmap<D>& other) : staticmap(other.staticmap), staticmap_owner(other.staticmap_owner), tree_map(other.tree_map) {};

        MyPmap<D>& operator=(const MyPmap<D>& other) {
            if (this != &other) {
                staticmap = other.staticmap;
                owner = other.owner;
                tree_map = other.tree_map;
            }
            return *this;
        };

        void print() const {
            tree_map->print();
        };

	/// Find the owner of a given key
        ProcessID owner(const KeyD& key) const {
            if (staticmap)
                return staticmap_owner;
            else {
                ProcessID owner;
                tree_map->find_owner(key, &owner);
                return owner;
            }
        };
    };

    /// The container in which the fascimile tree with its keys mapping to LBNodes is stored
    template <int D>
    class LBTree : public WorldContainer<typename DClass<D>::KeyD,typename DClass<D>::NodeD> {
        // No new variables necessary
    public:
        typedef WorldContainer<typename DClass<D>::KeyD,typename DClass<D>::NodeD> dcT;
        LBTree() {};
        LBTree(World& world, const SharedPtr< WorldDCPmapInterface<typename DClass<D>::KeyD> >& pmap) : dcT(world,pmap) {
            madness::print("LBTree(world, pmap) constructor");
            const MyPmap<D>* ppp = &(this->get_mypmap());
	    ppp->print();
            madness::print("LBTree(world, pmap) constructor (goodbye)");
        };
	/// Initialize the LBTree by converting a FunctionImpl to a LBTree
        template <typename T>
        inline void init_tree(const SharedPtr< FunctionImpl<T,D> >& f) {
            for (typename FunctionImpl<T,D>::dcT::iterator it = f->coeffs.begin(); it != f->coeffs.end(); ++it) {
            	// convert Node to LBNode
            	NodeData nd;
		typename DClass<D>::KeyD key = it->first;
            	if (!(it->second.has_children())) {
                	typename DClass<D>::NodeD lbnode(nd,false);
                	// insert into "this"
                	this->insert(key, lbnode);
            	} else {
                	typename DClass<D>::NodeD lbnode(nd,true);
                	// insert into "this"
                	this->insert(key, lbnode);
                }
            }
        };

        // Methods:
        void print(typename DClass<D>::KeyDConst& key) {
            typename DClass<D>::treeT::iterator it = this->find(key);
            if (it == this->end()) return;
            for (Level i = 0; i < key.level(); i++) cout << "  ";
            madness::print(key, it->second);
            for (KeyChildIterator<D> kit(key); kit; ++kit) {
                print(kit.key());
            }
        };

        Cost fix_cost(typename DClass<D>::KeyDConst& key);

        Cost depth_first_partition(typename DClass<D>::KeyDConst& key,
                                 vector<typename DClass<D>::TreeCoords>* klist, unsigned int npieces,
                                 Cost totalcost = 0, Cost *maxcost = 0);

//        void rollup(typename DClass<D>::KeyDConst& key);
        void rollup();

//        void meld(typename DClass<D>::KeyDConst& key);
        void meld(typename DClass<D>::treeT::iterator it);

        Cost make_partition(typename DClass<D>::KeyDConst& key,
                           vector<typename DClass<D>::KeyD>* klist, Cost partition_size,
                           bool last_partition, Cost used_up, bool *atleaf);

        void remove_cost(typename DClass<D>::KeyDConst& key, Cost c);

        Cost compute_cost(typename DClass<D>::KeyDConst& key);

        // inherited methods
        typename WorldContainer<typename DClass<D>::KeyD,typename DClass<D>::NodeD>::iterator 
        end() {
            return WorldContainer<typename DClass<D>::KeyD, typename DClass<D>::NodeD>::end();
        };

        typename WorldContainer<typename DClass<D>::KeyD,typename DClass<D>::NodeD>::iterator
        find(typename DClass<D>::KeyDConst& key) {
            return WorldContainer<typename DClass<D>::KeyD, typename DClass<D>::NodeD>::find(key);
        };

//         const SharedPtr<WorldDCPmapInterface< typename DClass<D>::KeyD >& get_pmap() {
//             return WorldContainer<typename DClass<D>::KeyD, typename DClass<D>::NodeD>::get_pmap();
//         };

        MyPmap<D>& get_mypmap() {
            return *static_cast< MyPmap<D>* >(this->get_pmap().get());
        };

    };

    /// Implementation of load balancing

    /// Implements the load balancing algorithm upon the tree underlying a function.
    template <typename T, int D>
    class LoadBalImpl {
    private:
	typedef MyPmap<D> Pmap;
        Function<T,D> f;
        SharedPtr<typename DClass<D>::treeT> skeltree;

        void construct_skel(SharedPtr<FunctionImpl<T,D> > f) {
            skeltree = SharedPtr<typename DClass<D>::treeT>(new typename DClass<D>::treeT(f->world,
                       f->coeffs.get_pmap()));
            typename DClass<D>::KeyD root(0);
//            madness::print("about to initialize tree");
	    skeltree->template init_tree<T>(f);
//            madness::print("just initialized tree");
        };

    public:
        //Constructors
        LoadBalImpl() {};

        LoadBalImpl(Function<T,D> f) : f(f) {
//            madness::print("LoadBalImpl (Function) constructor: f.impl", &f.get_impl());
            construct_skel(f.get_impl());
        };

        ~LoadBalImpl() {};

        //Methods

	/// Returns a shared pointer to a new process map, which can then be used to redistribute the function
        SharedPtr< WorldDCPmapInterface< Key<D> > > load_balance() {
            return SharedPtr< WorldDCPmapInterface< Key<D> > >(new MyPmap<D>(f.get_impl()->world, find_best_partition()));
        };

        vector<typename DClass<D>::TreeCoords> find_best_partition();
    };

    CompCost compute_comp_cost(Cost c, int n);

    Cost compute_partition_size(Cost cost, unsigned int parts);

}

#endif
