import os
import json
import requests
import sys

def main():
    api_key = os.environ.get("AI_API_KEY")
    intent = os.environ.get("INTENT")

    if not api_key or not intent:
        print("Errore: Variabili d'ambiente AI_API_KEY o INTENT mancanti.")
        sys.exit(1)

    # Utilizziamo l'endpoint REST standard. Puoi usare gemini-1.5-pro o modelli successivi
    url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-pro:generateContent?key={api_key}"

    prompt = f"""
    Sei un Senior Embedded Software Engineer. Il tuo compito è analizzare la seguente richiesta per un progetto PlatformIO (C++ per ESP32, repository: ha-ecoflow-ble) e dividerla in task sequenziali per un agente di coding AI subordinato (Jules).
    
    Richiesta dell'utente: "{intent}"
    
    Devi scomporre l'intento in step logici e granulari (es. 1. Aggiorna l'header, 2. Implementa la logica nel .cpp, 3. Aggiungi il log seriale).
    
    Rispondi SOLO con un array JSON.
    Formato richiesto per ogni oggetto dell'array:
    - "task_id": numero intero sequenziale
    - "description": stringa con le istruzioni esatte per l'agente AI su cosa scrivere o modificare
    - "target_files": array di stringhe con i percorsi dei file presumibilmente coinvolti (es. "src/main.cpp", "include/config.h")
    """

    # Payload con l'istruzione di forzare il MIME type in application/json
    payload = {
        "contents": [{"parts": [{"text": prompt}]}],
        "generationConfig": {
            "response_mime_type": "application/json",
            "temperature": 0.2 # Bassa temperatura per maggiore precisione logica
        }
    }

    print("Contattando l'AI per la scomposizione dei task...")
    response = requests.post(url, json=payload)
    
    try:
        response.raise_for_status()
        data = response.json()
        
        # Estrazione del contenuto testuale (che grazie al JSON mode è un JSON puro)
        json_output = data['candidates'][0]['content']['parts'][0]['text']
        
        # Salvataggio su file tasks.json per il passaggio successivo del workflow
        with open("tasks.json", "w") as f:
            f.write(json_output)
            
        print("Scomposizione completata. File tasks.json generato con successo!")
        
        # Stampa di debug visibile nei log di GitHub Actions
        print(json.dumps(json.loads(json_output), indent=2))
        
    except Exception as e:
        print(f"Errore durante la generazione dei task: {e}")
        if 'response' in locals():
            print(f"Dettagli API: {response.text}")
        sys.exit(1)

if __name__ == "__main__":
    main()
