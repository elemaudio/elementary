use serde::{Deserialize, Serialize};
use std::hash::{DefaultHasher, Hash, Hasher};

#[derive(Serialize, Deserialize)]
pub struct NodeRepr {
    pub hash: i32,
    pub kind: String,
    pub props: serde_json::Map<String, serde_json::Value>,
    pub output_channel: u32,
    pub children: Vec<NodeRepr>,
}

pub fn create_node(
    kind: &str,
    props: serde_json::Map<String, serde_json::Value>,
    children: Vec<NodeRepr>,
) -> NodeRepr {
    let mut hasher = DefaultHasher::new();

    kind.hash(&mut hasher);
    props.hash(&mut hasher);

    for child in children.iter() {
        child.hash.hash(&mut hasher);
    }

    NodeRepr {
        hash: hasher.finish() as i32,
        kind: kind.to_string(),
        props,
        output_channel: 0,
        children,
    }
}

pub struct ShallowNodeRepr {
    hash: i32,
    kind: String,
    props: serde_json::Map<String, serde_json::Value>,
    output_channel: u32,
    children: Vec<i32>,
}

impl From<&NodeRepr> for ShallowNodeRepr {
    fn from(node: &NodeRepr) -> ShallowNodeRepr {
        ShallowNodeRepr {
            hash: node.hash,
            kind: node.kind.clone(),
            props: node.props.clone(),
            output_channel: node.output_channel,
            children: node.children.iter().map(|n| n.hash).collect::<Vec<i32>>(),
        }
    }
}
