import pandas as pd
import matplotlib.pyplot as plt

# --- CONFIGURATION ---
FILE_1 = 'omnitestmikkel.csv' 
FILE_2 = 'Dirtestmikkel.csv'  # <--- Change this to your second filename

NODE_ID = 3

# Params for File 1
SKIP_1, END_1 = 700, 1150

# Params for File 2
SKIP_2 = 700
END_2 = 1200 # Set to a number if you want a specific end row for file 2

def load_and_filter(path, skip, end, node_id):
    """Helper function to load and clean the data"""
    df = pd.read_csv(path)
    # Slice the dataframe
    df = df.iloc[skip:end] if end else df.iloc[skip:]
    
    # Filter by Node ID
    df = df[df['node_id'] == node_id].copy()
    
    # Convert time
    if not df.empty:
        df['timestamp'] = pd.to_datetime(df['timestamp'])
    return df

def process_telemetry():
    try:
        # 1. Process Data 1
        df1 = load_and_filter(FILE_1, SKIP_1, END_1, NODE_ID)
        
        # 2. Process Data 2
        df2 = load_and_filter(FILE_2, SKIP_2, END_2, NODE_ID)

        if df1.empty and df2.empty:
            print("No data found for the selected criteria.")
            return

        # 3. Setup Plotting
        plt.figure(figsize=(12, 7))

        # Plot Dataset 1
        if not df1.empty:
            mean1 = df1['P_Signal'].mean()
            plt.plot(df1['timestamp'], df1['P_Signal'], label=f'File 1 (Mean: {mean1:.2f})', color='blue', alpha=0.8)
            plt.axhline(y=mean1, color='blue', linestyle='--', alpha=0.3)
            print(f"File 1 Mean: {mean1:.4f}")

        # Plot Dataset 2
        if not df2.empty:
            mean2 = df2['P_Signal'].mean()
            plt.plot(df2['timestamp'], df2['P_Signal'], label=f'File 2 (Mean: {mean2:.2f})', color='green', alpha=0.8)
            plt.axhline(y=mean2, color='green', linestyle='--', alpha=0.3)
            print(f"File 2 Mean: {mean2:.4f}")

        # 4. Finalize Visuals
        plt.title(f'Comparison of P_Signal (Node {NODE_ID})')
        plt.xlabel('Time')
        plt.ylabel('P_Signal Value')
        plt.legend()
        plt.grid(True, linestyle=':', alpha=0.6)
        plt.xticks(rotation=45)
        plt.tight_layout()

        plt.savefig('comparison_plot.png')
        print("\nComparison plot saved as 'comparison_plot.png'")
        plt.show()

    except FileNotFoundError as e:
        print(f"Error: Could not find file: {e.filename}")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    process_telemetry()