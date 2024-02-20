pub struct Runtime {}

impl Runtime {}

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("elementary/src/include/Runtime.h");
    }
}

pub fn say_name() -> String {
    "Elementary".to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        let result = say_name();
        assert_eq!(result, "Elementary");
    }
}
