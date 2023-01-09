export abstract class EmptyList {
  protected opaque: any;
}

export abstract class Cons<T> {
  protected opaque!: T;
}

export type list<T> = Cons<T> | EmptyList;
