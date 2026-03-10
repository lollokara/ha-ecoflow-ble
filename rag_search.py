import sys
import pickle
import faiss
import json
import argparse
from sentence_transformers import SentenceTransformer

def main():
    parser = argparse.ArgumentParser(description="Search the RAG index")
    parser.add_argument("query", help="The query string to search for")
    args = parser.parse_args()

    query = args.query

    print("Loading index...")
    try:
        index = faiss.read_index("rag_index/embeddings/embeddings.faiss")
        with open("rag_index/embeddings/embeddings.pkl", "rb") as f:
            metadata = pickle.load(f)
    except Exception as e:
        print(f"Error loading index: {e}")
        sys.exit(1)

    model = SentenceTransformer("all-MiniLM-L6-v2")
    q_emb = model.encode([query], show_progress_bar=False, convert_to_numpy=True)

    D, I = index.search(q_emb, 10)

    print("\n--- Results ---\n")
    for dist, idx in zip(D[0], I[0]):
        if idx == -1: continue
        path = metadata["paths"][idx]
        text = metadata["texts"][idx]

        print(f"File: {path}")
        print(f"Distance: {dist:.4f}")
        print("Snippet:")
        print("-" * 40)
        print(text[:300] + "..." if len(text) > 300 else text)
        print("-" * 40 + "\n")

if __name__ == "__main__":
    main()
