import os
import sys
import requests

def main():
    if len(sys.argv) < 2:
        print("Errore: Devi passare il percorso del file di log come argomento.")
        sys.exit(1)

    log_file_path = sys.argv[1]
    
    try:
        with open(log_file_path, "r", encoding="utf-8", errors="replace") as f:
            logs = f.read()
    except Exception as e:
        print(f"Errore durante la lettura del file di log: {e}")
        sys.exit(1)

    api_key = os.environ.get("AI_API_KEY")
    if not api_key:
        print("Errore: Variabile d'ambiente AI_API_KEY mancante.")
        sys.exit(1)

    url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-pro-latest:generateContent?key={api_key}"
    # Tronchiamo i log se dovessero essere eccezionalmente lunghi, 
    # mantenendo la parte finale (dove di solito ci sono gli errori seriali e di linker)
    max_chars = 100000
    if len(logs) > max_chars:
        logs = "...[LOG TRONCATI]...\n" + logs[-max_chars:]

    prompt = f"""
    Sei un Senior Embedded C++ Engineer. Un agente AI subordinato ha appena modificato il codice di un progetto PlatformIO per ESP32.
    Analizza i seguenti log di esecuzione. I log contengono sia la fase di build/upload, sia l'output del monitor seriale.

    Genera un report in formato Markdown chiaro e conciso, ottimizzato per la lettura su smartphone via Telegram.
    
    Struttura richiesta:
    1. **Stato Compilazione**: 🟢 Successo oppure 🔴 Fallita.
    2. **Stato Esecuzione**: 🟢 Stabile oppure 🔴 Anomalie Rilevate.
    3. **Dettagli**: 
       - Se la compilazione è fallita, individua il file e l'errore di sintassi/linker.
       - Se l'esecuzione ha problemi, individua stack smashes, kernel panic, boot loop o eccezioni dal monitor seriale.
    4. **Azione Consigliata**: Un suggerimento rapido su come risolvere il problema.

    Rispondi SOLO con il report in Markdown, senza preamboli.

    === INIZIO LOG ===
    {logs}
    === FINE LOG ===
    """

    payload = {
        "contents": [{"parts": [{"text": prompt}]}],
        "generationConfig": {
            "temperature": 0.1 # Molto bassa per un'analisi fattuale e zero allucinazioni
        }
    }

    try:
        response = requests.post(url, json=payload)
        response.raise_for_status()
        data = response.json()
        
        testo_report = data['candidates'][0]['content']['parts'][0]['text']
        
        # Stampa a schermo: questo output verrà catturato dalla Action e salvato in ai_feedback.md
        print(testo_report)
        
    except Exception as e:
        print("Errore API durante l'analisi dei log.")
        print(f"Dettagli: {e}")
        if 'response' in locals():
            print(f"Risposta server: {response.text}")
        sys.exit(1)

if __name__ == "__main__":
    main()
