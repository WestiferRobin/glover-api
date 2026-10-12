#ifndef NODE_HPP
#define NODE_HPP

class Node {
public:
    virtual ~Node() = default;
    virtual void run() = 0;
};

#endif // NODE_HPP
